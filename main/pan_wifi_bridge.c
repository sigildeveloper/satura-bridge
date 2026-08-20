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
#include "watchdog.h"

static const char *TAG = "satura_bridge";

// ============================================================
// Config
// ============================================================

#define NUM_DHCP_ENTRY          4
#define DNS_PORT                53

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

    nat_bridge_subscribe();
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

    watchdog_start();

    http_server_start();
    bt_pan_init();
    bt_pan_start();

    return 0;
}
