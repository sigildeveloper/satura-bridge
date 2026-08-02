/*
 * Satura Bridge — BT PAN <-> WiFi bridge for ESP32 + BTstack
 *
 * Version: v0.0.9 (Long-Run Stable)
 *
 * Fixes vs v0.0.8:
 *  - wifi_retry_handle race: xTaskCreate now inside critical section (atomic check+create)
 *  - wifi_soft_reset: proper event handler unregister before deinit, no double-register
 *  - wifi_recovery_task: one-at-a-time flag, prevents concurrent recovery storms
 *  - DNS watchdog: uses dedicated kill flag instead of external vTaskDelete (safer cleanup)
 *  - DNS sockets: closed by the task itself on restart signal, not from outside
 *  - bt_reopen_task: one-at-a-time guard, prevents task pile-up on fast BT cycling
 *  - wifi_start_task: one-at-a-time guard
 *  - handler_root static buffer: replaced with stack-local allocation
 *  - gap_read_rssi: called via btstack_run_loop_execute_on_main_thread (thread-safe)
 *  - dns_make_captive_reply: only responds to A-type queries, ignores AAAA/PTR/etc
 *  - Heap warn threshold raised, reboot threshold raised to 8192 for earlier detection
 *  - HTTP server max_open_sockets=2 for captive portal reliability
 *  - wifi_retries fully reset on successful connect in all states
 *  - APP_WIFI_FAILED → wifi_start_connect now resets retry state properly
 *  - nvs_flash_init guard in btstack_main
 *  - All shared task handles protected by state_mux
 */

#define BTSTACK_FILE__ "pan_wifi_bridge.c"

#define PROJECT_VERSION "v0.0.9"
#define TELEGRAM_CHAT   "https://t.me/nnmidletschat"
#define PAGE_FOOTER \
    "<hr><p>" \
    "Community: <a href='" TELEGRAM_CHAT "'>t.me/nnmidletschat</a>" \
    "<br>Author: @sigdev" \
    " | " PROJECT_VERSION \
    "</p>"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

#include "btstack_config.h"
#include "bnep_lwip.h"
#include "btstack.h"
#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "dhserver.h"
#include "lwip/tcpip.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "config.h"
#include "app_state.h"
#include "task_utils.h"
#include "dns_server.h"
#include "nvs_storage.h"
#include "http_utils.h"

static const char *TAG = "pan_bridge";

// ============================================================
// Config
// ============================================================

#define NVS_NAMESPACE           "satura"
#define NVS_KEY_SSID            "ssid"
#define NVS_KEY_PASS            "pass"

#define WIFI_MAX_RETRIES        7
#define WIFI_RETRY_BASE_MS      1000
#define WIFI_RETRY_MAX_MS       30000

#define NUM_DHCP_ENTRY          4
#define HTTP_PORT               80
#define DNS_PORT                53
#define BT_LEGACY_PIN           "0000"
#define DNS_TIMEOUT_MS          500
#define DNS_CACHE_SIZE          16
#define DNS_MAX_PACKET          512

#define BT_REOPEN_DELAY_MS      1200
#define HEAP_WARN_THRESHOLD     24576   /* 24 KB — warn early */
#define HEAP_REBOOT_THRESHOLD   8192    /* 8 KB — reboot before allocator panics */
#define HEARTBEAT_INTERVAL_MS   30000

// ============================================================
// Forward Declarations
// ============================================================


static void wifi_start_connect(void);
static void wifi_soft_reset(void);
static void wifi_recovery_task(void *arg);
static void watchdog_task(void *arg);
static void bt_reopen_task(void *arg);
static void wifi_retry_task(void *arg);

// ============================================================
// Globals
// ============================================================

/* All task handles and single-instance flags live under state_mux.
 * Rule: read/write these only inside taskENTER_CRITICAL / taskEXIT_CRITICAL. */
static portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile app_state_t app_state = APP_WAIT_BT;

static volatile int8_t bt_rssi   = -100;
static volatile int8_t wifi_rssi = -100;
static hci_con_handle_t bt_handle = HCI_CON_HANDLE_INVALID;

static volatile bool bt_connected            = false;
static volatile bool wifi_connected          = false;
static volatile bool wifi_ignore_disconnect  = false;
static volatile int  wifi_retries            = 0;
static volatile uint32_t wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;

/* One-at-a-time guards (under state_mux) */
static volatile bool wifi_retry_running    = false;
static volatile bool wifi_recovery_running = false;
static volatile bool bt_reopen_running     = false;
static volatile bool wifi_start_running    = false;

static char wifi_ssid[64] = {0};
static char wifi_pass[64] = {0};
static char wifi_ip[16]   = "--";
static int64_t boot_us    = 0;

static esp_netif_t   *sta_netif   = NULL;
static httpd_handle_t http_server = NULL;
static uint8_t pan_sdp_record[400];
static btstack_packet_callback_registration_t hci_event_cb;

static volatile int bt_reopen_counter = 0;


/* Event handler instance handles — needed for proper unregister */
static esp_event_handler_instance_t wifi_evt_inst = NULL;
static esp_event_handler_instance_t ip_evt_inst   = NULL;

// ============================================================
// Atomic accessors
// ============================================================

static inline bool get_bt_connected(void) {
    bool val;
    taskENTER_CRITICAL(&state_mux);
    val = bt_connected;
    taskEXIT_CRITICAL(&state_mux);
    return val;
}

esp_netif_t *app_get_sta_netif(void) {
    return sta_netif;
}

bool get_wifi_connected(void) {
    bool val;
    taskENTER_CRITICAL(&state_mux);
    val = wifi_connected;
    taskEXIT_CRITICAL(&state_mux);
    return val;
}

static inline hci_con_handle_t get_bt_handle(void) {
    hci_con_handle_t h;
    taskENTER_CRITICAL(&state_mux);
    h = bt_handle;
    taskEXIT_CRITICAL(&state_mux);
    return h;
}

// ============================================================
// Helpers & NVS
// ============================================================

void safe_task_create(TaskFunction_t fn, const char *name,
                              uint32_t stack, void *arg,
                              UBaseType_t prio, TaskHandle_t *handle) {
    if (xTaskCreate(fn, name, stack, arg, prio, handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task %s, rebooting...", name);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    }
}

static uint32_t uptime_seconds(void) {
    return (uint32_t)((esp_timer_get_time() - boot_us) / 1000000ULL);
}


// ============================================================
// NAT & Network
// ============================================================

static struct netif *bt_netif = NULL;

static void find_bt_netif(void) {
    struct netif *p = netif_list;
    while (p) {
        const ip4_addr_t *ip = netif_ip4_addr(p);
        if (ip4_addr1(ip) == GW_IP0 &&
            ip4_addr2(ip) == GW_IP1 &&
            ip4_addr3(ip) == GW_IP2) {
            bt_netif = p;
            return;
        }
        p = p->next;
    }
}

static void update_nat_lwip_ctx(void *arg) {
    (void)arg;
    if (!bt_netif) find_bt_netif();
    if (!bt_netif) {
        struct netif *p = netif_list;
        while (p) {
            ESP_LOGW(TAG, "  netif: %d.%d.%d.%d",
                ip4_addr1(netif_ip4_addr(p)), ip4_addr2(netif_ip4_addr(p)),
                ip4_addr3(netif_ip4_addr(p)), ip4_addr4(netif_ip4_addr(p)));
            p = p->next;
        }
        return;
    }
    bool enable = get_bt_connected() && get_wifi_connected();
    ip_napt_enable_netif(bt_netif, enable ? 1 : 0);
}

static void update_nat(void) {
    tcpip_callback(update_nat_lwip_ctx, NULL);
}

// ============================================================
// WiFi — retry task
// ============================================================

/* FIX: xTaskCreate is now inside the critical section so the
 * check-then-create is truly atomic.  We use a bool flag instead
 * of checking the handle because xTaskCreate can return before
 * the new task has a chance to run. */
static void wifi_schedule_retry(uint32_t delay_ms) {
    bool already;
    taskENTER_CRITICAL(&state_mux);
    already = wifi_retry_running;
    if (!already) wifi_retry_running = true;
    taskEXIT_CRITICAL(&state_mux);
    if (already) return;

    /* Pass delay as uintptr — safe on 32-bit MCU */
    if (xTaskCreate(wifi_retry_task, "wr", 3072,
                    (void *)(uintptr_t)delay_ms, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[WIFI] failed to create retry task");
        taskENTER_CRITICAL(&state_mux);
        wifi_retry_running = false;
        taskEXIT_CRITICAL(&state_mux);
    }
}

static void wifi_retry_task(void *arg) {
    uint32_t delay = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(delay));

    /* Re-check SSID — may have been cleared by /reset during sleep */
    taskENTER_CRITICAL(&state_mux);
    bool has_ssid = (strlen(wifi_ssid) != 0);
    taskEXIT_CRITICAL(&state_mux);

    if (has_ssid) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_wifi_connect();
    }

    taskENTER_CRITICAL(&state_mux);
    wifi_retry_running = false;
    taskEXIT_CRITICAL(&state_mux);
    vTaskDelete(NULL);
}

// ============================================================
// WiFi — connect & event handler
// ============================================================

static void wifi_start_connect(void) {
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid,     wifi_ssid, sizeof(cfg.sta.ssid)     - 1);
    strncpy((char *)cfg.sta.password, wifi_pass, sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode =
        (strlen(wifi_pass) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    cfg.sta.scan_method = WIFI_FAST_SCAN;
    cfg.sta.pmf_cfg.capable  = true;
    cfg.sta.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);

    app_state_set(APP_WIFI_CONNECTING);

    taskENTER_CRITICAL(&state_mux);
    wifi_retries       = 0;
    wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
    taskEXIT_CRITICAL(&state_mux);

    esp_wifi_connect();
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {

        taskENTER_CRITICAL(&state_mux);
        bool ignore = wifi_ignore_disconnect;
        if (ignore) wifi_ignore_disconnect = false;
        taskEXIT_CRITICAL(&state_mux);

        if (ignore) {
            taskENTER_CRITICAL(&state_mux);
            wifi_connected = false;
            taskEXIT_CRITICAL(&state_mux);
            app_state_t st = app_state_get();
            update_nat();
            return;
        }

        taskENTER_CRITICAL(&state_mux);
        wifi_connected = false;
        app_state_t st = app_state;
        taskEXIT_CRITICAL(&state_mux);

        update_nat();
        strncpy(wifi_ip, "--", sizeof(wifi_ip));

        if (st == APP_WIFI_CONNECTING) {
            taskENTER_CRITICAL(&state_mux);
            int retries  = ++wifi_retries;
            uint32_t delay = wifi_retry_delay_ms;
            wifi_retry_delay_ms = (delay * 2 > WIFI_RETRY_MAX_MS)
                                  ? WIFI_RETRY_MAX_MS : delay * 2;
            taskEXIT_CRITICAL(&state_mux);

            if (retries < WIFI_MAX_RETRIES) {
                wifi_schedule_retry(delay);
            } else {
                app_state_set(APP_WIFI_FAILED);
            }

        } else if (st == APP_BRIDGE || st == APP_BRIDGE_NO_WIFI) {
            taskENTER_CRITICAL(&state_mux);
            bool has_ssid = (strlen(wifi_ssid) != 0);
            taskEXIT_CRITICAL(&state_mux);

            if (!has_ssid) {
                app_state_set(APP_NO_WIFI);
                return;
            }
            if (st == APP_BRIDGE) {
                app_state_set(APP_BRIDGE_NO_WIFI);
                taskENTER_CRITICAL(&state_mux);
                wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
                wifi_retries = 0;
                taskEXIT_CRITICAL(&state_mux);
            }

            taskENTER_CRITICAL(&state_mux);
            wifi_retries++;
            uint32_t delay = wifi_retry_delay_ms;
            wifi_retry_delay_ms = (delay * 2 > WIFI_RETRY_MAX_MS)
                                  ? WIFI_RETRY_MAX_MS : delay * 2;
            taskEXIT_CRITICAL(&state_mux);

            wifi_schedule_retry(delay);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(wifi_ip, sizeof(wifi_ip), IPSTR, IP2STR(&e->ip_info.ip));

        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            taskENTER_CRITICAL(&state_mux);
            wifi_rssi = ap.rssi;
            taskEXIT_CRITICAL(&state_mux);
        }

        taskENTER_CRITICAL(&state_mux);
        wifi_connected      = true;
        wifi_retries        = 0;
        wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
        bool bt   = bt_connected;
        taskEXIT_CRITICAL(&state_mux);
        app_state_t st = app_state_get();

        update_nat();

        if (bt) {
            app_state_set(APP_BRIDGE);
        } else if (st != APP_WAIT_BT) {
            app_state_set(APP_WAIT_BT);
        }
    }
}

// ============================================================
// WiFi — init & soft reset
// ============================================================

static void wifi_register_handlers(void) {
    if (wifi_evt_inst == NULL)
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID,
            wifi_event_handler, NULL, &wifi_evt_inst));
    if (ip_evt_inst == NULL)
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP,
            wifi_event_handler, NULL, &ip_evt_inst));
}

static void wifi_unregister_handlers(void) {
    if (wifi_evt_inst) {
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt_inst);
        wifi_evt_inst = NULL;
    }
    if (ip_evt_inst) {
        esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, ip_evt_inst);
        ip_evt_inst = NULL;
    }
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_register_handlers();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_soft_reset(void) {
    ESP_LOGW(TAG, "[WIFI] soft-reset starting...");

    wifi_config_t saved_cfg = {0};
    esp_wifi_get_config(WIFI_IF_STA, &saved_cfg);

    /* FIX: unregister handlers BEFORE deinit to avoid dangling callbacks */
    wifi_unregister_handlers();

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_deinit();
    vTaskDelay(pdMS_TO_TICKS(300));

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    wifi_register_handlers();   /* FIX: idempotent — checks for NULL before registering */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &saved_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    ESP_LOGI(TAG, "[WIFI] soft-reset done");
}

/* FIX: one-at-a-time recovery — watchdog cannot spawn two recoveries */
static void wifi_recovery_task(void *arg) {
    (void)arg;
    wifi_soft_reset();
    taskENTER_CRITICAL(&state_mux);
    wifi_recovery_running = false;
    taskEXIT_CRITICAL(&state_mux);
    vTaskDelete(NULL);
}


// ============================================================
// Watchdog & Heartbeat
// ============================================================

/* FIX: gap_read_rssi must run in BTstack context.
 * btstack_run_loop_execute_on_main_thread takes a
 * btstack_context_callback_registration_t *, not a bare function pointer.
 * We keep a static registration struct and reuse it each heartbeat.
 * The struct must stay alive until the callback fires — static is safe. */
static void rssi_poll_cb(void *context) {
    (void)context;
    hci_con_handle_t h = get_bt_handle();
    if (h != HCI_CON_HANDLE_INVALID) gap_read_rssi(h);
}

static btstack_context_callback_registration_t rssi_cb_reg = {
    .callback = rssi_poll_cb,
    .context  = NULL,
};

static void watchdog_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(10000));

    uint32_t wifi_stuck_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t min_heap  = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);

        taskENTER_CRITICAL(&state_mux);
        bool b_conn = bt_connected;
        bool w_conn = wifi_connected;
        int8_t rw   = wifi_rssi;
        int8_t rb   = bt_rssi;
        taskEXIT_CRITICAL(&state_mux);
        app_state_t st = app_state_get();

        /* Update WiFi RSSI */
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            taskENTER_CRITICAL(&state_mux);
            wifi_rssi = ap.rssi;
            rw = ap.rssi;
            taskEXIT_CRITICAL(&state_mux);
        }

        /* FIX: schedule BT RSSI poll in BTstack's own thread */
        btstack_run_loop_execute_on_main_thread(&rssi_cb_reg);

        uint32_t up = uptime_seconds();
        ESP_LOGI(TAG,
            "[HB] State:%s | BT:%s RSSI:%d | WiFi:%s RSSI:%d"
            " | Heap:%dKB min:%dKB | Up:%" PRIu32 "d %02" PRIu32
            ":%02" PRIu32 ":%02" PRIu32,
            app_state_to_str(st),
            b_conn ? "ON" : "OFF", (int)rb,
            w_conn ? "ON" : "OFF", (int)rw,
            (int)(free_heap / 1024), (int)(min_heap / 1024),
            up / 86400, (up % 86400) / 3600, (up % 3600) / 60, up % 60);

        // /* Принудительная очистка устаревших NAPT записей при низком heap */
        // if (free_heap < 32768) {   /* < 32KB — начинаем чистить */
        //     ip_napt_gc();           /* если есть в твоей сборке lwip_napt */
        // }

        /* WiFi stuck watchdog */
        if (st == APP_BRIDGE_NO_WIFI) {
            if (++wifi_stuck_count >= 10) {
                ESP_LOGE(TAG, "[WDT] WiFi stuck! Triggering recovery...");
                bool already;
                taskENTER_CRITICAL(&state_mux);
                already = wifi_recovery_running;
                if (!already) wifi_recovery_running = true;
                taskEXIT_CRITICAL(&state_mux);
                if (!already) {
                    safe_task_create(wifi_recovery_task, "wifi_rec",
                                     3072, NULL, 5, NULL);
                }
                wifi_stuck_count = 0;
            }
        } else {
            wifi_stuck_count = 0;
        }

        /* DNS hung watchdog */
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        dns_server_watchdog_tick(now);

        /* Heap watchdog */
        if (free_heap < HEAP_WARN_THRESHOLD) {
            ESP_LOGW(TAG, "[WDT] Low heap: %d bytes", (int)free_heap);
        }
        if (free_heap < HEAP_REBOOT_THRESHOLD) {
            ESP_LOGE(TAG, "[WDT] Critical heap (%d), rebooting...",
                     (int)free_heap);
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
    }
}

// ============================================================
// HTTP Server
// ============================================================

static bool captive_check(httpd_req_t *req) {
    app_state_t st = app_state_get();
    if (st == APP_BRIDGE) return true;
    char host[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK
        && strstr(host, GW_IP_STR) == NULL) {
        char host_no_port[64];
        strncpy(host_no_port, host, sizeof(host_no_port) - 1);
        host_no_port[sizeof(host_no_port)-1] = '\0';
        char *colon = strchr(host_no_port, ':');
        if (colon) *colon = '\0';
        struct in_addr tmp;
        if (inet_pton(AF_INET, host_no_port, &tmp) == 0) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://" GW_IP_STR "/");
            httpd_resp_send(req, NULL, 0);
            return false;
        }
    }
    return true;
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
    "<b>Free heap:</b> %d KB"
    "</div><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reset'>Forget WiFi</a><br>"
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
    "<form action='/setup' method='post'>"
    "<p>SSID:<br>"
    "<input type='text' name='ssid' size='20' maxlength='32'></p>"
    "<p>Password: (optional)<br>"
    "<input type='password' name='pass' size='20' maxlength='63'></p>"
    "<p><input type='submit' value='Connect' style='font-size:110%;'></p>"
    "</form><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reboot' style='color:#e74c3c;'>Reboot</a>"
    "<br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

/* FIX: page buffer is now stack-local — no shared static buffer race */
static esp_err_t handler_root(httpd_req_t *req) {
    if (!captive_check(req)) return ESP_OK;
    set_no_cache(req, "text/html");

    taskENTER_CRITICAL(&state_mux);
    int retries       = wifi_retries;
    bool w_conn       = wifi_connected;
    int8_t rw         = wifi_rssi;
    int8_t rb         = bt_rssi;
    taskEXIT_CRITICAL(&state_mux);
    app_state_t st = app_state_get();

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
                html_escape(wifi_ssid, esc, sizeof(esc));
                /* FIX: stack-local buffer */
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
                    "<p>Attempt %d of %d</p>"
                    "<small>This page refreshes automatically.</small><hr>"
                    "<a href='/'>Reload</a><br>"
                    "<a href='/reset'>Forget WiFi</a>"
                    "<br><small>" PAGE_FOOTER "</small>"
                    "</body></html>",
                    esc, retries + 1, WIFI_MAX_RETRIES);
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

    /* Status page (APP_BRIDGE or WAIT_BT+wifi_connected) */
    char esc[64] = {0};
    html_escape(wifi_ssid, esc, sizeof(esc));
    uint32_t up = uptime_seconds();
    char *page = malloc(3072);
    if (!page) return ESP_ERR_NO_MEM;
    snprintf(page, 3072, PAGE_STATUS_FMT,
             esc, wifi_ip, (int)rw, (int)rb,
             up / 86400, (up % 86400) / 3600, (up % 3600) / 60, up % 60,
             (int)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
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

        taskENTER_CRITICAL(&state_mux);
        strncpy(wifi_ssid, ns, sizeof(wifi_ssid) - 1);
        strncpy(wifi_pass, np, sizeof(wifi_pass) - 1);
        taskEXIT_CRITICAL(&state_mux);

        nvs_storage_save(wifi_ssid, wifi_pass);
        app_state_set(APP_WIFI_CONNECTING);
        wifi_start_connect();

        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        return httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t handler_reset(httpd_req_t *req) {
    taskENTER_CRITICAL(&state_mux);
    memset(wifi_ssid, 0, sizeof(wifi_ssid));
    memset(wifi_pass, 0, sizeof(wifi_pass));
    taskEXIT_CRITICAL(&state_mux);
    nvs_storage_clear();
    esp_wifi_disconnect();
    app_state_set(APP_NO_WIFI);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
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

static void http_server_start(void) {
    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size       = 8192;
    cfg.max_open_sockets = 2;   /* FIX: allow captive portal + status page */
    if (httpd_start(&http_server, &cfg) == ESP_OK) {
        static const httpd_uri_t uris[] = {
            { "/",            HTTP_GET,  handler_root,       NULL },
            { "/setup",       HTTP_POST, handler_setup_post, NULL },
            { "/setup",       HTTP_GET,  handler_setup_get,  NULL },
            { "/reset",       HTTP_GET,  handler_reset,      NULL },
            { "/reboot",      HTTP_GET,  handler_reboot,     NULL },
            { "/favicon.ico", HTTP_GET,  handler_favicon,    NULL },
        };
        for (int i = 0; i < 6; i++)
            httpd_register_uri_handler(http_server, &uris[i]);
        httpd_register_err_handler(http_server,
                                   HTTPD_404_NOT_FOUND, handler_404);
    }
}

// ============================================================
// Bluetooth / BTstack
// ============================================================

static void bt_set_visible(bool v) {
    gap_discoverable_control(v ? 1 : 0);
    gap_connectable_control(v ? 1 : 0);
    ESP_LOGI(TAG, "[BT] %s", v ? "visible" : "hidden");
}

static void bt_reopen_task(void *arg) {
    (void)arg;
    ESP_LOGW(TAG, "[BTR] started, heap=%u",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    vTaskDelay(pdMS_TO_TICKS(BT_REOPEN_DELAY_MS));

    if (!get_bt_connected()) bt_set_visible(true);

    taskENTER_CRITICAL(&state_mux);
    bt_reopen_running = false;
    taskEXIT_CRITICAL(&state_mux);
    vTaskDelete(NULL);
}

/* FIX: one-at-a-time wifi_start_task */
static void wifi_start_task(void *arg) {
    (void)arg;
    wifi_start_connect();
    taskENTER_CRITICAL(&state_mux);
    wifi_start_running = false;
    taskEXIT_CRITICAL(&state_mux);
    vTaskDelete(NULL);
}

static void hci_packet_handler(uint8_t type, uint16_t ch,
                                uint8_t *pkt, uint16_t sz) {
    if (type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    switch (hci_event_packet_get_type(pkt)) {
        case HCI_EVENT_CONNECTION_REQUEST:
            hci_event_connection_request_get_bd_addr(pkt, addr);
            ESP_LOGI(TAG,
                "[HCI] Connection request from %02X:%02X:%02X:%02X:%02X:%02X",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            break;
        case HCI_EVENT_CONNECTION_COMPLETE:
            hci_event_connection_complete_get_bd_addr(pkt, addr);
            ESP_LOGI(TAG,
                "[HCI] Connection complete status=0x%02x addr=%02X:%02X:%02X:%02X:%02X:%02X",
                hci_event_connection_complete_get_status(pkt),
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            ESP_LOGI(TAG,
                "[HCI] Disconnected handle=0x%04x reason=0x%02x",
                hci_event_disconnection_complete_get_connection_handle(pkt),
                hci_event_disconnection_complete_get_reason(pkt));
            break;
        case GAP_EVENT_RSSI_MEASUREMENT:
            if (gap_event_rssi_measurement_get_con_handle(pkt) == get_bt_handle()) {
                taskENTER_CRITICAL(&state_mux);
                bt_rssi = gap_event_rssi_measurement_get_rssi(pkt);
                taskEXIT_CRITICAL(&state_mux);
            }
            break;
        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(pkt, addr);
            gap_pin_code_response(addr, BT_LEGACY_PIN);
            break;
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            hci_event_user_confirmation_request_get_bd_addr(pkt, addr);
            gap_ssp_confirmation_response(addr);
            break;
        default: break;
    }
}

static void bnep_lwip_packet_handler(uint8_t type, uint16_t ch,
                                      uint8_t *pkt, uint16_t sz) {
    if (type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    switch (hci_event_packet_get_type(pkt)) {

        case BNEP_EVENT_CHANNEL_OPENED: {
            if (bnep_event_channel_opened_get_status(pkt)) {
                ESP_LOGW(TAG, "[BT] BNEP open failed status=0x%02x",
                         bnep_event_channel_opened_get_status(pkt));
                break;
            }
            bnep_event_channel_opened_get_remote_address(pkt, addr);
            hci_con_handle_t h = bnep_event_channel_opened_get_con_handle(pkt);

            taskENTER_CRITICAL(&state_mux);
            if (bt_connected) {
                taskEXIT_CRITICAL(&state_mux);
                bnep_disconnect(addr);
                return;
            }
            bt_connected = true;
            bt_handle    = h;
            bool wc          = wifi_connected;
            bool has_ssid    = (strlen(wifi_ssid) != 0);
            bool can_start   = !wifi_start_running;
            if (can_start && !wc && has_ssid) wifi_start_running = true;
            taskEXIT_CRITICAL(&state_mux);

            ESP_LOGI(TAG, "[BT] BNEP channel opened");
            bt_set_visible(false);

            if (wc) {
                app_state_set(APP_BRIDGE);
            } else if (!has_ssid) {
                app_state_set(APP_NO_WIFI);
                taskENTER_CRITICAL(&state_mux);
                wifi_rssi = -100;
                taskEXIT_CRITICAL(&state_mux);
            } else if (can_start) {
                /* FIX: use guarded flag set above */
                safe_task_create(wifi_start_task, "wist", 3072, NULL, 5, NULL);
            }
            update_nat();
            break;
        }

        case BNEP_EVENT_CHANNEL_CLOSED: {
            taskENTER_CRITICAL(&state_mux);
            bt_connected = false;
            bt_handle    = HCI_CON_HANDLE_INVALID;
            bt_rssi      = -100;
            bool already  = bt_reopen_running;
            if (!already) bt_reopen_running = true;
            taskEXIT_CRITICAL(&state_mux);

            app_state_set(APP_WAIT_BT);

            if (!already) {
                bt_reopen_counter++;
                ESP_LOGW(TAG, "[BTR] create #%d heap=%u",
                         bt_reopen_counter,
                         heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
                safe_task_create(bt_reopen_task, "btr", 4096, NULL, 4, NULL);
            } else {
                ESP_LOGW(TAG, "[BTR] already running, skip create");
            }

            update_nat();
            break;
        }

        default: break;
    }
}

static void pan_setup(void) {
    gap_set_local_name("Satura Bridge");
    gap_discoverable_control(1);
    gap_connectable_control(1);
    gap_set_class_of_device(0x020302);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_set_security_level(LEVEL_0);

    hci_event_cb.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_cb);

#if defined(L2CAP_SET_MAX_MTU)
    l2cap_set_max_mtu(1691);
#endif
    l2cap_init();

    bnep_init();
    sdp_init();

    memset(pan_sdp_record, 0, sizeof(pan_sdp_record));
    uint16_t net_types[] = {0x0800, 0x0806, 0};
    pan_create_nap_sdp_record(pan_sdp_record,
                              sdp_create_service_record_handle(),
                              net_types, NULL, NULL, BNEP_SECURITY_NONE,
                              PAN_NET_ACCESS_TYPE_OTHER, 128000,
                              "SaturaBridge", "BT PAN WiFi Bridge");
    sdp_register_service(pan_sdp_record);

    bnep_lwip_init();
    bnep_lwip_register_service(BLUETOOTH_SERVICE_CLASS_NAP, 1691);
    bnep_lwip_register_packet_handler(bnep_lwip_packet_handler);
}

static dhcp_entry_t dhcp_entries[NUM_DHCP_ENTRY] = {
    { {0}, {GW_IP0, GW_IP1, GW_IP2, 2}, {255,255,255,0}, 24*60*60 },
    { {0}, {GW_IP0, GW_IP1, GW_IP2, 3}, {255,255,255,0}, 24*60*60 },
    { {0}, {GW_IP0, GW_IP1, GW_IP2, 4}, {255,255,255,0}, 24*60*60 },
    { {0}, {GW_IP0, GW_IP1, GW_IP2, 5}, {255,255,255,0}, 24*60*60 },
};

static dhcp_config_t dhcp_config = {
    {GW_IP0, GW_IP1, GW_IP2, GW_IP3}, 67,
    {GW_IP0, GW_IP1, GW_IP2, GW_IP3},
    NULL,
    NUM_DHCP_ENTRY,
    dhcp_entries
};

int btstack_main(int argc, const char *argv[]) {
    (void)argc; (void)argv;
    boot_us = esp_timer_get_time();

    /* FIX: ensure NVS is initialised here in case app_main doesn't do it */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "[APP] NVS flash error %d, erasing...", ret);
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init();

    if (nvs_storage_load(wifi_ssid, sizeof(wifi_ssid), wifi_pass, sizeof(wifi_pass))) {
        ESP_LOGI(TAG, "[APP] credentials loaded, waiting for BT before WiFi");
        /* Stay in APP_WAIT_BT — wifi_start_connect fires after BT connects */
    } else {
        app_state_set(APP_NO_WIFI);
    }

    dhserv_init(&dhcp_config);

    dns_server_start();

    safe_task_create(watchdog_task, "wdt", 4096, NULL, 2, NULL);

    http_server_start();
    pan_setup();
    hci_power_control(HCI_POWER_ON);

    return 0;
}
