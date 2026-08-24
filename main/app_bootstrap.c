/*
 * Satura Bridge — application bootstrap.
 *
 * Owns app_main's counterpart on the BTstack side (btstack_main), the
 * DHCP config for the captive network, and safe_task_create(), the
 * shared xTaskCreate wrapper used by every other module. Startup
 * order: subscribe nat_bridge to the event bus, bring up WiFi (and
 * connect immediately if credentials are already saved), start the
 * DHCP/DNS/watchdog/HTTP services, then bring up the BT PAN stack.
 */

#define BTSTACK_FILE__ "app_bootstrap.c"

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

    /* Redundant with the nvs_flash_init() in main.c's app_main(), but
     * harmless — nvs_flash_init() is idempotent, and this keeps
     * btstack_main() safe to call on its own. */
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
