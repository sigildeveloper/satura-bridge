#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "lwip/sockets.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include "http_server.h"
#include "config.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "bt_pan.h"
#include "http_utils.h"
#include "task_utils.h"
#include "uptime.h"
#include "nvs_storage.h"
#include "proxy_gateway.h"

#define PROJECT_VERSION "v0.0.14"
#define TELEGRAM_CHAT   "https://t.me/nnmidletschat"
#define PAGE_FOOTER \
    "<hr><p>" \
    "Community: <a href='" TELEGRAM_CHAT "'>t.me/nnmidletschat</a>" \
    "<br>Author: @sigdev" \
    " | " PROJECT_VERSION \
    "</p>"


#define HEAD_BUF_SIZE 2048

static esp_err_t handler_proxy_relay(httpd_req_t *req);

static httpd_handle_t http_server = NULL;
static const char *TAG = "http_server";

static bool host_matches_bridge(const char *host_no_port) {
    if (strcmp(host_no_port, GW_IP_STR) == 0) return true;
    char sta_ip[16] = {0};
    wifi_manager_get_ip(sta_ip, sizeof(sta_ip));
    if (sta_ip[0] != '\0' && strcmp(sta_ip, "--") != 0 &&
        strcmp(host_no_port, sta_ip) == 0) return true;
    return false;
}

static bool captive_check(httpd_req_t *req) {
    app_state_t st = app_state_get();
    if (st == APP_BRIDGE) return true;
    char host[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK) {
        char host_no_port[64];
        strncpy(host_no_port, host, sizeof(host_no_port) - 1);
        host_no_port[sizeof(host_no_port)-1] = '\0';
        char *colon = strchr(host_no_port, ':');
        if (colon) *colon = '\0';

        if (!host_matches_bridge(host_no_port)) {
            struct in_addr tmp;
            if (inet_pton(AF_INET, host_no_port, &tmp) == 0) {
                httpd_resp_set_status(req, "302 Found");
                httpd_resp_set_hdr(req, "Location", "http://" GW_IP_STR "/");
                httpd_resp_send(req, NULL, 0);
                return false;
            }
        }
    }
    return true;
}

/* True if this request's Host header refers to something other than
 * the bridge itself — i.e. it's real internet traffic that arrived
 * here because proxy-mode DNS pointed everything at us. */
static bool is_foreign_host(httpd_req_t *req) {
    char host[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        return false;
    }
    char host_no_port[128];
    strncpy(host_no_port, host, sizeof(host_no_port) - 1);
    host_no_port[sizeof(host_no_port) - 1] = '\0';
    char *colon = strchr(host_no_port, ':');
    if (colon) *colon = '\0';
    return !host_matches_bridge(host_no_port);
}

/* ---- Static page templates ---- */

static const char PAGE_SETUP[] =
    "<html><head><title>Satura Bridge Setup</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Satura Bridge Setup</h2><hr>"
    "<p><a href='/networks'>Scan for networks</a></p>"
    "<p>...or enter manually:</p>"
    "<form action='/setup' method='post'>"
    "<p>SSID:<br>"
    "<input type='text' name='ssid' size='20' maxlength='32'></p>"
    "<p>Password: (optional)<br>"
    "<input type='password' name='pass' size='20' maxlength='63'></p>"
    "<p><input type='submit' value='Connect' style='font-size:110%;'></p>"
    "</form><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reboot' style='color:#e74c3c;'>Reboot</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_STATUS_FMT[] =
    "<html><head><title>Satura Bridge Status</title>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='30'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Satura Bridge</h2><hr>"
    "<div style='text-align:left;background:#ecf0f1;padding:15px;'>"
    "<b>WiFi:</b> %s<br>"
    "<b>IP:</b> %s<br>"
    "<b>WiFi RSSI:</b> %d dBm<br>"
    "<b>BT RSSI:</b> %d dBm<br>"
    "<b>Uptime:</b> %" PRIu32 "d %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "<br>"
    "<b>Free heap:</b> %d KB<br>"
    "<b>Proxy:</b> %s"
    "</div><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reset'>Forget WiFi</a><br>"
    "<a href='/networks'>Manage Networks</a><br>"
    "<a href='/proxy'>Proxy Gateway</a><br>"
    "<a href='/reboot' style='color:#e74c3c;'>Reboot</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_NO_WIFI_FMT[] =
    "<html><head><title>WiFi Lost</title>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='5'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>WiFi Lost</h2><hr>"
    "<p>Reconnecting...</p>"
    "<p>Attempt %d</p>"
    "<small>This page refreshes automatically.</small><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reset'>Forget WiFi</a>"
    "<br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_SETUP_FAILED[] =
    "<html><head><title>Connection Failed</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Connection Failed</h2><hr>"
    "<p style='color:#e74c3c;'>Could not connect.<br>Check SSID and password.</p>"
    "<p><a href='/networks'>Manage networks</a></p>"
    "<form action='/setup' method='post'>";

static const char PAGE_NETWORKS_HEADER[] =
    "<html><head><title>Wi-Fi Networks</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Wi-Fi Networks</h2><hr>";

static const char PAGE_NETWORKS_FOOTER[] =
    "<a href='/'>Back</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_PROXY_FMT[] =
    "<html><head><title>Proxy Gateway</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>HTTP Proxy Gateway</h2><hr>"
    "<p style='text-align:left;'>Redirects outgoing HTTP (port 80) connections "
    "to an external gateway or domain (e.g. a WAP compression gateway like 15pmm01.com). "
    "HTTPS traffic is not affected.</p>"
    "<form action='/proxy' method='post'>"
    "<p><label><input type='checkbox' name='en' value='1' %s> Enabled</label></p>"
    "<p>Gateway/domain host:<br>"
    "<input type='text' name='host' size='24' maxlength='63' value='%s'></p>"
    "<p>Gateway/domain port:<br>"
    "<input type='text' name='port' size='10' maxlength='5' value='%d'></p>"
    "<p><input type='submit' value='Save' style='font-size:110%%;'></p>"
    "</form><hr>"
    "<a href='/'>Back</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_CONNECT_PROMPT_FMT[] =
    "<html><head><title>Connect</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Connect to network</h2><hr>"
    "<p><b>%s</b></p>"
    "<form action='/networks/add' method='post'>"
    "<input type='hidden' name='ssid' value=\"%s\">"
    "<p>Password: (leave empty if open)<br>"
    "<input type='password' name='pass' size='20' maxlength='63' autofocus></p>"
    "<p><input type='submit' value='Add &amp; Connect' style='font-size:110%%;'></p>"
    "</form><hr>"
    "<a href='/networks'>Cancel</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static esp_err_t handler_root(httpd_req_t *req) {
    if (proxy_gateway_is_enabled() && is_foreign_host(req)) {
        return handler_proxy_relay(req);
    }
    if (!captive_check(req)) return ESP_OK;
    set_no_cache(req, "text/html");

    app_state_t st    = app_state_get();
    bool w_conn       = get_wifi_connected();
    int8_t rw         = get_wifi_rssi();
    int8_t rb         = get_bt_rssi();

    int retries, max_retries;
    wifi_manager_get_progress(&retries, &max_retries);

    bool show_status = (st == APP_BRIDGE) ||
                       (st == APP_WAIT_BT && w_conn);

    if (!show_status) {
        char esc[64] = {0};
        switch (st) {
            case APP_WAIT_BT:
            case APP_NO_WIFI:
                return httpd_resp_sendstr(req, PAGE_SETUP);
            case APP_WIFI_FAILED:
                return httpd_resp_sendstr(req, PAGE_SETUP_FAILED);
            case APP_WIFI_CONNECTING: {
                char ssid_buf[64];
                wifi_manager_get_ssid(ssid_buf, sizeof(ssid_buf));
                html_escape(ssid_buf, esc, sizeof(esc));
                char *page = malloc(2048);
                if (!page) return ESP_ERR_NO_MEM;
                snprintf(page, 2048,
                    "<html><head><title>Connecting...</title>"
                    "<meta charset='utf-8'>"
                    "<meta http-equiv='refresh' content='5'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<meta name='format-detection' content='telephone=no'>"
                    "</head>"
                    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
                    "<h2>Connecting</h2><hr>"
                    "<p>Network:</p><div style='word-wrap:break-word;'><b>%s</b></div>"
                    "<p>Trying network %d of %d</p>"
                    "<small>This page refreshes automatically.</small><hr>"
                    "<a href='/'>Reload</a><br>"
                    "<a href='/reset'>Forget WiFi</a>"
                    "<br><small>" PAGE_FOOTER "</small>"
                    "</body></html>",
                    esc, retries + 1, max_retries);
                esp_err_t r = httpd_resp_sendstr(req, page);
                free(page);
                return r;
            }
            case APP_BRIDGE_NO_WIFI: {
                char *page = malloc(1024);
                if (!page) return ESP_ERR_NO_MEM;
                snprintf(page, 1024, PAGE_NO_WIFI_FMT, retries + 1);
                esp_err_t r = httpd_resp_sendstr(req, page);
                free(page);
                return r;
            }
            default: break;
        }
    }

    char esc[64] = {0};
    char ssid_buf2[64];
    wifi_manager_get_ssid(ssid_buf2, sizeof(ssid_buf2));
    html_escape(ssid_buf2, esc, sizeof(esc));
    uint32_t up = uptime_seconds();
    char *page = malloc(3072);
    if (!page) return ESP_ERR_NO_MEM;
    char ip_buf[16];
    wifi_manager_get_ip(ip_buf, sizeof(ip_buf));
    snprintf(page, 3072, PAGE_STATUS_FMT,
                 esc, ip_buf, (int)rw, (int)rb,
                 up / 86400, (up % 86400) / 3600, (up % 3600) / 60, up % 60,
                 (int)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024),
                 proxy_gateway_is_enabled() ? "Enabled" : "Disabled");
    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static esp_err_t handler_setup_get(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_setup_post(httpd_req_t *req) {
    char buf[256] = {0};
    int rec = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (rec <= 0) return ESP_FAIL;
    buf[rec] = '\0';
    char ns[64] = {0}, np[64] = {0};
    if (httpd_query_key_value(buf, "ssid", ns, sizeof(ns)) == ESP_OK) {
        httpd_query_key_value(buf, "pass", np, sizeof(np));

        wifi_manager_set_credentials(ns, np);
        app_state_set(APP_WIFI_CONNECTING);
        wifi_manager_start_connect();

        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        return httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t handler_reset(httpd_req_t *req) {
    wifi_manager_clear_credentials();
    app_state_set(APP_NO_WIFI);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_proxy_get(httpd_req_t *req) {
    set_no_cache(req, "text/html");

    proxy_gateway_config_t cfg;
    proxy_gateway_load(&cfg);

    char *page = malloc(1536);
    if (!page) return ESP_ERR_NO_MEM;
    snprintf(page, 1536, PAGE_PROXY_FMT,
             cfg.enabled ? "checked" : "",
             cfg.host, (int)cfg.port);
    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static bool is_hop_by_hop_header(const char *name) {
    return strcasecmp(name, "Content-Length") == 0 ||
           strcasecmp(name, "Transfer-Encoding") == 0 ||
           strcasecmp(name, "Connection") == 0;
}

static void relay_bytes_httpd(httpd_req_t *req, int upstream_sock) {
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(upstream_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Read the upstream's status line + headers byte-by-byte until the
     * blank line that ends them. Heap-allocated — 2KB is too much to
     * safely add to an already-deep call stack inside the httpd task. */
    char *head = malloc(HEAD_BUF_SIZE);
    if (!head) return;
    head[0] = '\0';
    int head_len = 0;
    bool found_terminator = false;

    while (head_len < HEAD_BUF_SIZE - 1 && !found_terminator) {
        int n = recv(upstream_sock, head + head_len, HEAD_BUF_SIZE - 1 - head_len, 0);
        if (n <= 0) break;
        head_len += n;
        head[head_len] = '\0';

        /* Scan from a bit before this chunk started, in case the
            * \r\n\r\n terminator straddles two recv() calls. */
        int scan_from = head_len - n - 3;
        if (scan_from < 0) scan_from = 0;
        for (int i = scan_from; i + 4 <= head_len; i++) {
            if (head[i] == '\r' && head[i + 1] == '\n' &&
                head[i + 2] == '\r' && head[i + 3] == '\n') {
                found_terminator = true;
                break;
            }
        }
    }

    /* Parse "HTTP/1.1 200 OK" -> "200 OK" for httpd_resp_set_status */
    char status[32] = "200 OK";
    char *sp1 = strchr(head, ' ');
    if (sp1) {
        char *line_end = strstr(sp1, "\r\n");
        if (line_end) {
            size_t len = line_end - (sp1 + 1);
            if (len < sizeof(status)) {
                memcpy(status, sp1 + 1, len);
                status[len] = '\0';
            }
        }
    }
    httpd_resp_set_status(req, status);

    /* Forward every response header except hop-by-hop ones that
     * esp_http_server manages itself (we're using chunked transfer,
     * so Content-Length/Transfer-Encoding from upstream would conflict). */
    char *line_start = strstr(head, "\r\n");
    if (line_start) line_start += 2; /* skip past the status line */
    while (line_start && *line_start && strncmp(line_start, "\r\n", 2) != 0) {
        char *line_end = strstr(line_start, "\r\n");
        if (!line_end) break;

        char *colon = memchr(line_start, ':', line_end - line_start);
        if (colon) {
            char name[64] = {0};
            size_t name_len = colon - line_start;
            if (name_len < sizeof(name)) {
                memcpy(name, line_start, name_len);
                name[name_len] = '\0';

                if (!is_hop_by_hop_header(name)) {
                    char *val_start = colon + 1;
                    while (*val_start == ' ') val_start++;
                    size_t val_len = line_end - val_start;

                    /* httpd_resp_set_hdr() only stores the pointer, not a
                        * copy — the value must stay valid until the response
                        * is sent, so allocate it instead of using a stack
                        * buffer that goes out of scope. */
                    char *value = malloc(val_len + 1);
                    if (value) {
                        memcpy(value, val_start, val_len);
                        value[val_len] = '\0';
                        httpd_resp_set_hdr(req, name, value);
                        /* Intentionally not freed here — esp_http_server
                            * needs it until the response finishes sending,
                            * which happens right after this function returns.
                            * The whole request's memory is reclaimed when the
                            * connection's task exits. */
                    }
                }
            }
        }
        line_start = line_end + 2;
    }

    /* If the read above grabbed some body bytes along with the headers
    * (a single recv() can return more than just the header block),
    * forward that leftover chunk first before reading more. */
    int body_start = -1;
    for (int i = 0; i + 4 <= head_len; i++) {
        if (head[i] == '\r' && head[i + 1] == '\n' &&
            head[i + 2] == '\r' && head[i + 3] == '\n') {
            body_start = i + 4;
            break;
        }
    }
    if (body_start >= 0 && body_start < head_len) {
        httpd_resp_send_chunk(req, head + body_start, head_len - body_start);
    }
    free(head);

    /* Now relay the rest of the body. */
    char buf[512];
    while (1) {
        int n = recv(upstream_sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) break;
    }
    httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t handler_proxy_relay(httpd_req_t *req) {
    char host[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    char *colon = strchr(host, ':');
    if (colon) *colon = '\0';

    char upstream_host[64] = {0};
    uint16_t upstream_port = 0;
    proxy_gateway_get_upstream(upstream_host, sizeof(upstream_host), &upstream_port);
    if (upstream_host[0] == '\0' || upstream_port == 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int upstream_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (upstream_sock < 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    struct sockaddr_in upstream_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(upstream_port),
    };
    if (inet_pton(AF_INET, upstream_host, &upstream_addr.sin_addr) != 1 ||
        connect(upstream_sock, (struct sockaddr *)&upstream_addr, sizeof(upstream_addr)) != 0) {
        ESP_LOGW(TAG, "failed to connect to upstream %s:%d", upstream_host, upstream_port);
        close(upstream_sock);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    /* Rebuild the request line in absolute-URI form: GET http://host/uri HTTP/1.1 */
    const char *method = (req->method == HTTP_POST) ? "POST" : "GET";
    char req_line[1200];
    snprintf(req_line, sizeof(req_line), "%s http://%s%s HTTP/1.1\r\n",
             method, host, req->uri);
    send(upstream_sock, req_line, strlen(req_line), 0);

    char host_hdr[160];
    snprintf(host_hdr, sizeof(host_hdr), "Host: %s\r\n", host);
    send(upstream_sock, host_hdr, strlen(host_hdr), 0);

    /* Forward the phone's real headers — many WAP gateways use these to
     * distinguish legitimate mobile browsers, and Cookie/Content-Type
     * are needed for logins and form submissions to work at all. */
    static const char *forward_headers[] = {
        "User-Agent", "Accept", "Accept-Language", "Accept-Charset",
        "Cookie", "Content-Type", NULL
    };
    for (int i = 0; forward_headers[i]; i++) {
        char val[512] = {0};
        if (httpd_req_get_hdr_value_str(req, forward_headers[i], val, sizeof(val)) == ESP_OK) {
            char hdr_line[560];
            snprintf(hdr_line, sizeof(hdr_line), "%s: %s\r\n", forward_headers[i], val);
            send(upstream_sock, hdr_line, strlen(hdr_line), 0);
        }
    }

    /* Forward the request body for POST (form submissions, logins). */
    size_t remaining = req->content_len;
    if (remaining > 0) {
        char len_hdr[64];
        snprintf(len_hdr, sizeof(len_hdr), "Content-Length: %d\r\n", (int)remaining);
        send(upstream_sock, len_hdr, strlen(len_hdr), 0);
    }

    send(upstream_sock, "Connection: close\r\n\r\n", 22, 0);

    char body_buf[512];
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(body_buf) ? remaining : sizeof(body_buf);
        int n = httpd_req_recv(req, body_buf, chunk);
        if (n <= 0) break;
        send(upstream_sock, body_buf, n, 0);
        remaining -= n;
    }

    ESP_LOGI(TAG, "relaying: %s", req_line);

    relay_bytes_httpd(req, upstream_sock);

    shutdown(upstream_sock, SHUT_RDWR);
    close(upstream_sock);
    return ESP_OK;
}

static esp_err_t handler_proxy_post(httpd_req_t *req) {
    char buf[256] = {0};
    int rec = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (rec <= 0) return ESP_FAIL;
    buf[rec] = '\0';

    char host_s[64] = {0}, port_s[16] = {0}, en_s[8] = {0};
    bool has_en = (httpd_query_key_value(buf, "en", en_s, sizeof(en_s)) == ESP_OK);
    httpd_query_key_value(buf, "host", host_s, sizeof(host_s));
    httpd_query_key_value(buf, "port", port_s, sizeof(port_s));

    proxy_gateway_config_t cfg = {0};
    cfg.enabled = has_en;
    strncpy(cfg.host, host_s, sizeof(cfg.host) - 1);
    int port = atoi(port_s);
    cfg.port = (port > 0 && port < 65536) ? (uint16_t)port : 0;

    proxy_gateway_save(&cfg);
    proxy_gateway_invalidate_cache();

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/proxy");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_get(httpd_req_t *req) {
    set_no_cache(req, "text/html");

    char *page = malloc(4096);
    if (!page) return ESP_ERR_NO_MEM;
    size_t len = 0;

    len += snprintf(page + len, 4096 - len, "%s", PAGE_NETWORKS_HEADER);

    /* Saved networks */
    wifi_network_t saved[WIFI_MAX_SAVED_NETWORKS];
    int saved_count = wifi_manager_get_saved_networks(saved, WIFI_MAX_SAVED_NETWORKS);

    len += snprintf(page + len, 4096 - len,
        "<div style='text-align:left;background:#ecf0f1;padding:10px;'>"
        "<b>Saved (%d/%d):</b><br>", saved_count, WIFI_MAX_SAVED_NETWORKS);

    if (saved_count == 0) {
        len += snprintf(page + len, 4096 - len, "<i>None yet</i><br>");
    }
    for (int i = 0; i < saved_count; i++) {
        char esc[64] = {0};
        html_escape(saved[i].ssid, esc, sizeof(esc));
        len += snprintf(page + len, 4096 - len,
            "%s &nbsp; <a href='/networks/delete?idx=%d' style='color:#e74c3c;'>[remove]</a><br>",
            esc, i);
    }
    len += snprintf(page + len, 4096 - len, "</div><br>");

    /* Add network form */
    len += snprintf(page + len, 4096 - len,
        "<form action='/networks/add' method='post'>"
        "<p>SSID:<br><input type='text' name='ssid' size='20' maxlength='32'></p>"
        "<p>Password: (optional)<br><input type='password' name='pass' size='20' maxlength='63'></p>"
        "<p><input type='submit' value='Add Network' style='font-size:110%%;'></p>"
        "</form><hr>");

    /* Scan results */
    bool scanning = wifi_manager_scan_in_progress();
    if (scanning) {
        len += snprintf(page + len, 4096 - len,
            "<p>Scanning...</p>"
            "<meta http-equiv='refresh' content='2'>");
    } else {
        wifi_scan_result_t results[WIFI_MAX_SCAN_RESULTS];
        int n = wifi_manager_get_scan_results(results, WIFI_MAX_SCAN_RESULTS);

        len += snprintf(page + len, 4096 - len,
            "<div style='text-align:left;background:#ecf0f1;padding:10px;'>"
            "<b>Found nearby:</b><br>");
        if (n == 0) {
            len += snprintf(page + len, 4096 - len, "<i>No scan results yet</i><br>");
        }
        for (int i = 0; i < n && len < 3600; i++) {
            char esc[64] = {0};
            html_escape(results[i].ssid, esc, sizeof(esc));
            char enc[100] = {0};
            url_encode(results[i].ssid, enc, sizeof(enc));
            len += snprintf(page + len, 4096 - len,
                "<a href='/networks/connect?ssid=%s'>%s (%d dBm)</a>%s<br>",
                enc, esc, (int)results[i].rssi,
                results[i].saved ? " &nbsp;<i>saved</i>" : "");
        }
        len += snprintf(page + len, 4096 - len, "</div><br>");
        len += snprintf(page + len, 4096 - len,
            "<a href='/networks/scan'>Scan again</a><br>");
    }

    len += snprintf(page + len, 4096 - len, "%s", PAGE_NETWORKS_FOOTER);

    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static esp_err_t handler_networks_add_post(httpd_req_t *req) {
    char buf[256] = {0};
    int rec = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (rec <= 0) return ESP_FAIL;
    buf[rec] = '\0';
    char ns[64] = {0}, np[64] = {0};
    if (httpd_query_key_value(buf, "ssid", ns, sizeof(ns)) == ESP_OK) {
        httpd_query_key_value(buf, "pass", np, sizeof(np));
        wifi_manager_add_network(ns, np);
        app_state_set(APP_WIFI_CONNECTING);
        wifi_manager_start_connect();
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_delete(httpd_req_t *req) {
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char idx_s[8] = {0};
        if (httpd_query_key_value(query, "idx", idx_s, sizeof(idx_s)) == ESP_OK) {
            int idx = atoi(idx_s);
            wifi_manager_remove_network(idx);
        }
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/networks");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_scan(httpd_req_t *req) {
    wifi_manager_scan_start();
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/networks");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_connect_get(httpd_req_t *req) {
    set_no_cache(req, "text/html");

    char query[128] = {0};
    char ssid_raw[100] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char ssid_enc[100] = {0};
        if (httpd_query_key_value(query, "ssid", ssid_enc, sizeof(ssid_enc)) == ESP_OK) {
            url_decode(ssid_enc, ssid_raw, sizeof(ssid_raw));
        }
    }

    if (strlen(ssid_raw) == 0) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/networks");
        return httpd_resp_send(req, NULL, 0);
    }

    char esc[64] = {0};
    html_escape(ssid_raw, esc, sizeof(esc));

    char *page = malloc(1536);
    if (!page) return ESP_ERR_NO_MEM;
    snprintf(page, 1536, PAGE_CONNECT_PROMPT_FMT, esc, esc);
    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static void reboot_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static esp_err_t handler_reboot(httpd_req_t *req) {
    httpd_resp_sendstr(req,
        "<html><body><p>Rebooting...</p></body></html>");
    safe_task_create(reboot_task, "reboot", 2048, NULL, 3, NULL);
    return ESP_OK;
}

static esp_err_t handler_favicon(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_404(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" GW_IP_STR "/");
    return httpd_resp_send(req, NULL, 0);
}

void http_server_start(void) {
    httpd_config_t cfg    = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size        = 12288;
    cfg.max_open_sockets  = 2;
    cfg.max_uri_handlers  = 16;
    cfg.uri_match_fn      = httpd_uri_match_wildcard;
    if (httpd_start(&http_server, &cfg) == ESP_OK) {
        static const httpd_uri_t uris[] = {
            { "/",                 HTTP_GET,  handler_root,               NULL },
            { "/setup",            HTTP_POST, handler_setup_post,         NULL },
            { "/setup",            HTTP_GET,  handler_setup_get,          NULL },
            { "/reset",            HTTP_GET,  handler_reset,              NULL },
            { "/reboot",           HTTP_GET,  handler_reboot,             NULL },
            { "/favicon.ico",      HTTP_GET,  handler_favicon,            NULL },
            { "/networks",         HTTP_GET,  handler_networks_get,       NULL },
            { "/networks/add",     HTTP_POST, handler_networks_add_post,  NULL },
            { "/networks/delete",  HTTP_GET,  handler_networks_delete,    NULL },
            { "/networks/scan",    HTTP_GET,  handler_networks_scan,      NULL },
            { "/networks/connect", HTTP_GET,  handler_networks_connect_get, NULL },
            { "/proxy",            HTTP_GET,  handler_proxy_get,          NULL },
            { "/proxy",            HTTP_POST, handler_proxy_post,         NULL },
            { "/*",                HTTP_GET,  handler_proxy_relay,        NULL },
            { "/*",                HTTP_POST, handler_proxy_relay,        NULL },
        };
        for (int i = 0; i < 14; i++)
            httpd_register_uri_handler(http_server, &uris[i]);
        httpd_register_err_handler(http_server,
                                   HTTPD_404_NOT_FOUND, handler_404);
    }
}
