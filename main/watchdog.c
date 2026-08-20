#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uptime.h"

#include "watchdog.h"
#include "app_state.h"
#include "task_utils.h"
#include "wifi_manager.h"
#include "bt_pan.h"
#include "dns_server.h"

static const char *TAG = "satura_bridge";

#define HEAP_WARN_THRESHOLD    24576
#define HEAP_REBOOT_THRESHOLD  8192
#define HEARTBEAT_INTERVAL_MS  30000

static void watchdog_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(10000));

    uint32_t wifi_stuck_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t min_heap  = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);

        app_state_t st = app_state_get();
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

void watchdog_start(void) {
    safe_task_create(watchdog_task, "wdt", 4096, NULL, 2, NULL);
}
