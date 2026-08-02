#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "app_state.h"

static const char *TAG = "app_state";
static portMUX_TYPE app_state_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile app_state_t current_state = APP_WAIT_BT;

const char *app_state_to_str(app_state_t st) {
    switch (st) {
        case APP_WAIT_BT:         return "WAIT_BT";
        case APP_NO_WIFI:         return "NO_WIFI";
        case APP_WIFI_CONNECTING: return "WIFI_CONN";
        case APP_WIFI_FAILED:     return "WIFI_FAIL";
        case APP_BRIDGE:          return "BRIDGE_ACTIVE";
        case APP_BRIDGE_NO_WIFI:  return "BRIDGE_LOST_WIFI";
        default:                  return "UNKNOWN";
    }
}

app_state_t app_state_get(void) {
    app_state_t val;
    taskENTER_CRITICAL(&app_state_mux);
    val = current_state;
    taskEXIT_CRITICAL(&app_state_mux);
    return val;
}

void app_state_set(app_state_t new_state) {
    app_state_t old_state;
    bool changed = false;
    taskENTER_CRITICAL(&app_state_mux);
    old_state = current_state;
    if (old_state != new_state) {
        current_state = new_state;
        changed = true;
    }
    taskEXIT_CRITICAL(&app_state_mux);
    if (changed)
        ESP_LOGI(TAG, "[STATE] %s -> %s",
                 app_state_to_str(old_state), app_state_to_str(new_state));
}
