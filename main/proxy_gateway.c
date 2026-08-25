#include <string.h>
#include <errno.h>
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "dns_server.h"
#include "config.h"
#include "task_utils.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "proxy_gateway.h"

static const char *TAG = "dns_server";

#define DNS_PORT             53
#define DNS_TIMEOUT_MS       500
#define DNS_CACHE_SIZE       16
#define DNS_MAX_PACKET       512
#define DNS_WATCHDOG_MS      15000
#define DNS_WATCHDOG_TICKS   2

typedef struct {
    bool     valid;
    uint16_t hash;
    uint16_t qlen;
    uint16_t rlen;
    uint32_t saved_ms;
    uint8_t  query[DNS_MAX_PACKET];
    uint8_t  reply[DNS_MAX_PACKET];
} dns_cache_entry_t;

static portMUX_TYPE dns_mux = portMUX_INITIALIZER_UNLOCKED;

static int dns_srv_sock = -1;
static int dns_ext_sock = -1;
static dns_cache_entry_t dns_cache[DNS_CACHE_SIZE];
static uint8_t dns_cache_next = 0;
static TaskHandle_t dns_task_handle = NULL;
static volatile uint32_t dns_last_alive_ms = 0;
static volatile bool     dns_restart_flag  = false;
static uint32_t dns_stuck_count = 0;

static void dns_server_task(void *arg);

static uint16_t dns_query_hash(const uint8_t *q, int qlen) {
    uint32_t h = 0;
    for (int i = 12; i < qlen && i < 40; i++) h = h * 31 + q[i];
    return (uint16_t)(h ^ (h >> 16));
}

static bool dns_cache_lookup(const uint8_t *query, int qlen,
                              uint8_t *reply, int *rlen) {
    if (qlen < 12) return false;
    uint16_t qhash = dns_query_hash(query, qlen);
    uint32_t now   = (uint32_t)(esp_timer_get_time() / 1000ULL);
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        dns_cache_entry_t *e = &dns_cache[i];
        if (!e->valid || e->hash != qhash || e->qlen != (uint16_t)qlen) continue;
        if ((uint32_t)(now - e->saved_ms) > 60000) { e->valid = false; continue; }
        if (qlen > 12 && memcmp(e->query + 12, query + 12, qlen - 12) != 0) continue;
        memcpy(reply, e->reply, e->rlen);
        reply[0] = query[0];
        reply[1] = query[1];
        *rlen = e->rlen;
        return true;
    }
    return false;
}

static void dns_cache_store(const uint8_t *query, int qlen,
                             const uint8_t *reply, int rlen) {
    if (qlen < 12 || qlen > DNS_MAX_PACKET ||
        rlen < 12 || rlen > DNS_MAX_PACKET) return;
    dns_cache_entry_t *e = &dns_cache[dns_cache_next];
    dns_cache_next = (dns_cache_next + 1) % DNS_CACHE_SIZE;
    e->valid    = true;
    e->hash     = dns_query_hash(query, qlen);
    e->qlen     = (uint16_t)qlen;
    e->rlen     = (uint16_t)rlen;
    e->saved_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    memcpy(e->query, query, qlen);
    memcpy(e->reply, reply, rlen);
    e->query[0] = e->query[1] = e->reply[0] = e->reply[1] = 0;
}

static int dns_make_captive_reply(const uint8_t *query, int qlen,
                                   uint8_t *reply, int rmax) {
    if (qlen < 12 || rmax < 28) return 0;

    int off = 12;
    while (off < qlen - 4) {
        uint8_t len = query[off];
        if (len == 0) { off++; break; }
        if ((len & 0xC0) == 0xC0) { off += 2; break; }
        off += 1 + len;
    }
    if (off + 4 > qlen) return 0;
    uint16_t qtype = (query[off] << 8) | query[off + 1];

    if (qtype != 0x0001) {
        if (rmax < 12) return 0;
        if (qlen > 200) qlen = 200;
        memcpy(reply, query, qlen > 12 ? 12 : qlen);
        reply[0] = query[0]; reply[1] = query[1];
        reply[2] = 0x81; reply[3] = 0x83;
        reply[4] = 0x00; reply[5] = 0x01;
        reply[6] = 0x00; reply[7] = 0x00;
        reply[8] = 0x00; reply[9] = 0x00;
        reply[10]= 0x00; reply[11]= 0x00;
        int rlen = 12;
        int qsz  = qlen - 12;
        if (qsz > 0 && rlen + qsz <= rmax) {
            memcpy(reply + rlen, query + 12, qsz);
            rlen += qsz;
        }
        return rlen;
    }

    if (qlen > 200) qlen = 200;
    reply[0] = query[0]; reply[1] = query[1];
    reply[2] = 0x81; reply[3] = 0x80;
    reply[4] = 0x00; reply[5] = 0x01;
    reply[6] = 0x00; reply[7] = 0x01;
    reply[8] = 0x00; reply[9] = 0x00;
    reply[10]= 0x00; reply[11]= 0x00;
    int rlen     = 12;
    int qsection = qlen - 12;
    if (qsection <= 0 || rlen + qsection + 16 > rmax) return 0;
    memcpy(reply + rlen, query + 12, qsection);
    rlen += qsection;
    reply[rlen++] = 0xC0; reply[rlen++] = 0x0C;
    reply[rlen++] = 0x00; reply[rlen++] = 0x01;
    reply[rlen++] = 0x00; reply[rlen++] = 0x01;
    reply[rlen++] = 0x00; reply[rlen++] = 0x00;
    reply[rlen++] = 0x00; reply[rlen++] = 0x01;
    reply[rlen++] = 0x00; reply[rlen++] = 0x04;
    reply[rlen++] = GW_IP0; reply[rlen++] = GW_IP1;
    reply[rlen++] = GW_IP2; reply[rlen++] = GW_IP3;
    return rlen;
}

static bool dns_forward(int ext_sock, struct sockaddr_in *ext_dns,
                         const uint8_t *query, int qlen,
                         uint8_t *reply, int *rlen) {
    if (sendto(ext_sock, query, qlen, 0,
               (struct sockaddr *)ext_dns, sizeof(*ext_dns)) < 0) return false;
    struct sockaddr_in from;
    socklen_t fl = sizeof(from);
    int n = recvfrom(ext_sock, reply, DNS_MAX_PACKET, 0,
                     (struct sockaddr *)&from, &fl);
    /* dns_ext_sock is unconnected, so recvfrom() accepts a UDP datagram
     * from ANY source — without checking that it actually came from the
     * upstream server we queried, a matching 2-byte transaction ID
     * (65536 possibilities, trivially guessable/floodable by anyone on
     * the same WiFi network) would be enough to spoof a DNS reply and
     * redirect this phone's traffic anywhere, including into the proxy
     * relay path that forwards Cookie/Content-Type/login form data. */
    if (n >= 12 && reply[0] == query[0] && reply[1] == query[1] &&
        from.sin_addr.s_addr == ext_dns->sin_addr.s_addr &&
        from.sin_port == ext_dns->sin_port) {
        *rlen = n; return true;
    }
    return false;
}

static void dns_get_upstream(struct sockaddr_in *out) {
    out->sin_family = AF_INET;
    out->sin_port   = htons(53);
    esp_netif_dns_info_t dns_info = {0};
    esp_netif_t *sta = wifi_manager_get_sta_netif();
    if (sta &&
        esp_netif_get_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK
        && dns_info.ip.u_addr.ip4.addr != 0) {
        out->sin_addr.s_addr = dns_info.ip.u_addr.ip4.addr;
    } else {
        inet_pton(AF_INET, FALLBACK_DNS, &out->sin_addr);
    }
}

static void dns_server_task(void *arg) {
    (void)arg;

    dns_restart_flag = false;

    dns_srv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (dns_srv_sock < 0) {
        ESP_LOGE(TAG, "[DNS] failed to open srv sock: %d", errno);
        taskENTER_CRITICAL(&dns_mux);
        dns_task_handle = NULL;
        taskEXIT_CRITICAL(&dns_mux);
        vTaskDelete(NULL);
        return;
    }
    dns_ext_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (dns_ext_sock < 0) {
        ESP_LOGE(TAG, "[DNS] failed to open ext sock: %d", errno);
        close(dns_srv_sock);
        dns_srv_sock = -1;
        taskENTER_CRITICAL(&dns_mux);
        dns_task_handle = NULL;
        taskEXIT_CRITICAL(&dns_mux);
        vTaskDelete(NULL);
        return;
    }

    struct timeval tv  = {1, 0};
    struct timeval tv2 = {0, DNS_TIMEOUT_MS * 1000};
    setsockopt(dns_srv_sock, SOL_SOCKET, SO_RCVTIMEO, &tv,  sizeof(tv));
    setsockopt(dns_ext_sock, SOL_SOCKET, SO_RCVTIMEO, &tv2, sizeof(tv2));

    struct sockaddr_in local = {
        .sin_family      = AF_INET,
        .sin_port        = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    if (bind(dns_srv_sock, (struct sockaddr *)&local, sizeof(local)) != 0) {
        ESP_LOGE(TAG, "[DNS] bind failed: %d", errno);
        close(dns_srv_sock);
        close(dns_ext_sock);
        dns_srv_sock = dns_ext_sock = -1;
        taskENTER_CRITICAL(&dns_mux);
        dns_task_handle = NULL;
        taskEXIT_CRITICAL(&dns_mux);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[DNS] task started");

    while (1) {
        if (dns_restart_flag) {
            ESP_LOGW(TAG, "[DNS] restart flag — shutting down task");
            break;
        }

        dns_last_alive_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

        uint8_t query_buf[DNS_MAX_PACKET], reply_buf[DNS_MAX_PACKET];
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int qlen = recvfrom(dns_srv_sock, query_buf, sizeof(query_buf),
                            0, (struct sockaddr *)&client, &clen);
        if (qlen < 12) continue;

        struct sockaddr_in ext_dns;
        dns_get_upstream(&ext_dns);

        int rlen = 0;
        bool have_reply = false;

        if (proxy_gateway_is_enabled() && app_state_get() == APP_BRIDGE) {
            /* Proxy mode active: answer every A-record query with our own
                * IP, so all HTTP traffic naturally lands on our web server,
                * which will forward it to the configured upstream proxy. */
            rlen = dns_make_captive_reply(query_buf, qlen,
                                            reply_buf, sizeof(reply_buf));
            have_reply = (rlen > 0);
        } else {
            have_reply = dns_cache_lookup(query_buf, qlen, reply_buf, &rlen);
            if (!have_reply) {
                if (get_wifi_connected() &&
                    dns_forward(dns_ext_sock, &ext_dns,
                                query_buf, qlen, reply_buf, &rlen)) {
                    dns_cache_store(query_buf, qlen, reply_buf, rlen);
                    have_reply = true;
                }
            }
            if (!have_reply) {
                rlen = dns_make_captive_reply(query_buf, qlen,
                                                reply_buf, sizeof(reply_buf));
            }
        }
        if (rlen > 0) {
            sendto(dns_srv_sock, reply_buf, rlen, 0,
                   (struct sockaddr *)&client, clen);
        }
    }

    close(dns_srv_sock);
    close(dns_ext_sock);
    dns_srv_sock = dns_ext_sock = -1;
    taskENTER_CRITICAL(&dns_mux);
    dns_task_handle = NULL;
    taskEXIT_CRITICAL(&dns_mux);
    vTaskDelete(NULL);
}

void dns_server_start(void) {
    dns_last_alive_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    TaskHandle_t h = NULL;
    safe_task_create(dns_server_task, "dns", 4096, NULL, 6, &h);
    taskENTER_CRITICAL(&dns_mux);
    dns_task_handle = h;
    taskEXIT_CRITICAL(&dns_mux);
}

void dns_server_watchdog_tick(uint32_t now_ms) {
    TaskHandle_t h;
    taskENTER_CRITICAL(&dns_mux);
    h = dns_task_handle;
    taskEXIT_CRITICAL(&dns_mux);

    if (h != NULL && (now_ms - dns_last_alive_ms) > DNS_WATCHDOG_MS) {
        if (++dns_stuck_count >= DNS_WATCHDOG_TICKS) {
            ESP_LOGE(TAG, "[WDT] DNS hang! Signalling restart...");

            dns_restart_flag = true;
            taskENTER_CRITICAL(&dns_mux);
            dns_task_handle = NULL;
            taskEXIT_CRITICAL(&dns_mux);

            vTaskDelay(pdMS_TO_TICKS(1500));

            memset(dns_cache, 0, sizeof(dns_cache));
            dns_cache_next = 0;
            dns_last_alive_ms = now_ms;

            TaskHandle_t new_h = NULL;
            safe_task_create(dns_server_task, "dns", 4096, NULL, 6, &new_h);
            taskENTER_CRITICAL(&dns_mux);
            dns_task_handle = new_h;
            taskEXIT_CRITICAL(&dns_mux);

            dns_stuck_count = 0;
        }
    } else {
        dns_stuck_count = 0;
    }
}
