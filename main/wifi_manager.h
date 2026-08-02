#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_netif.h"

/* Lifecycle */
void wifi_manager_init(void);
void wifi_manager_start_connect(void);
void wifi_manager_schedule_recovery(void);

/* Credentials */
void wifi_manager_set_credentials(const char *ssid, const char *pass);
void wifi_manager_clear_credentials(void);
bool wifi_manager_has_credentials(void);
void wifi_manager_load_saved_credentials(void);

/* Status accessors for HTTP/UI */
void wifi_manager_get_ssid(char *out, size_t len);
void wifi_manager_get_ip(char *out, size_t len);
int  wifi_manager_get_retries(void);

esp_netif_t *wifi_manager_get_sta_netif(void);

int wifi_manager_get_max_retries(void);

typedef void (*wifi_manager_state_cb_t)(void);
void wifi_manager_set_state_change_cb(wifi_manager_state_cb_t cb);
