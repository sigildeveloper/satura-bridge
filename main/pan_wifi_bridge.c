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

#include <errno.h>

#include "dhserver.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "config.h"
#include "app_state.h"
#include "task_utils.h"
#include "dns_server.h"
#include "wifi_manager.h"
#include "bt_pan.h"
#include "http_server.h"
#include "uptime.h"
#include "esp_timer.h"
#include "nat_bridge.h"

static const char *TAG = "satura_bridge";

// ============================================================
// Config
// ============================================================

#define NUM_DHCP_ENTRY          4
#define DNS_PORT                53
#define HEAP_WARN_THRESHOLD     24576
#define HEAP_REBOOT_THRESHOLD   8192
#define HEARTBEAT_INTERVAL_MS   30000

// ============================================================
// Forward Declarations
// ============================================================

static void watchdog_task(void *arg);

// ============================================================
// Helpers
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

// ============================================================
// Watchdog & Heartbeat
// ============================================================

static void watchdog_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(10000));

    uint32_t wifi_stuck_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t min_heap  = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);

        app_state_t st    = app_state_get();
        bool b_conn = get_bt_connected();
        bool w_conn = get_wifi_connected();
        int8_t rw   = get_wifi_rssi();
        int8_t rb   = get_bt_rssi();

        /* Update WiFi RSSI */
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            set_wifi_rssi(ap.rssi);
            rw = ap.rssi;
        }

        /* FIX: schedule BT RSSI poll in BTstack's own thread */
        bt_pan_request_rssi_poll();

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
        // if (free_heap < 32768) {
        //     ip_napt_gc();
        // }

        /* WiFi stuck watchdog */
        if (st == APP_BRIDGE_NO_WIFI) {
            if (++wifi_stuck_count >= 10) {
                ESP_LOGE(TAG, "[WDT] WiFi stuck! Triggering recovery...");
                wifi_manager_schedule_recovery();
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
// DHCP Config
// ============================================================

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
    uptime_init();

    /* FIX: ensure NVS is initialised here in case app_main doesn't do it */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "[APP] NVS flash error %d, erasing...", ret);
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_manager_set_state_change_cb(nat_bridge_update);
    bt_pan_set_state_change_cb(nat_bridge_update);
    wifi_manager_init();

    wifi_manager_load_saved_credentials();
    if (wifi_manager_has_credentials()) {
        ESP_LOGI(TAG, "[APP] credentials loaded, connecting to WiFi immediately");
        wifi_manager_start_connect();
    } else {
        app_state_set(APP_NO_WIFI);
    }

    dhserv_init(&dhcp_config);

    dns_server_start();

    safe_task_create(watchdog_task, "wdt", 4096, NULL, 2, NULL);

    http_server_start();
    bt_pan_init();
    bt_pan_start();

    return 0;
}
