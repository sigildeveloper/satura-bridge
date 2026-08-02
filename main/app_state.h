#pragma once
#include <stdbool.h>
#include "esp_netif.h"

typedef enum {
    APP_WAIT_BT,
    APP_NO_WIFI,
    APP_WIFI_CONNECTING,
    APP_WIFI_FAILED,
    APP_BRIDGE,
    APP_BRIDGE_NO_WIFI,
} app_state_t;

app_state_t app_state_get(void);
void        app_state_set(app_state_t new_state);
const char *app_state_to_str(app_state_t st);

bool get_wifi_connected(void);
esp_netif_t *app_get_sta_netif(void);
