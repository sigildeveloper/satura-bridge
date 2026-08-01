#pragma once
#include <stdbool.h>
#include "esp_netif.h"

bool get_wifi_connected(void);
esp_netif_t *app_get_sta_netif(void);
