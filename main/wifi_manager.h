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
void wifi_manager_set_credentials(const char *ssid, const char *pass, bool hidden);
void wifi_manager_clear_credentials(void);
bool wifi_manager_has_credentials(void);
void wifi_manager_load_saved_credentials(void);

/* Status accessors for HTTP/UI */
void wifi_manager_get_ssid(char *out, size_t len);
void wifi_manager_get_ip(char *out, size_t len);

esp_netif_t *wifi_manager_get_sta_netif(void);

void wifi_manager_get_progress(int *current, int *total);

#include "nvs_storage.h"

#define WIFI_MAX_SCAN_RESULTS 20

typedef struct {
    char    ssid[33];
    int8_t  rssi;
    bool    saved;   /* true if this SSID is already in the saved list */
} wifi_scan_result_t;

void wifi_manager_scan_start(void);
int  wifi_manager_get_scan_results(wifi_scan_result_t *out, int max_count);
bool wifi_manager_scan_in_progress(void);

/* Multi-network management */
int  wifi_manager_get_saved_networks(wifi_network_t *out, int max_count);
bool wifi_manager_add_network(const char *ssid, const char *pass, bool hidden);
bool wifi_manager_remove_network(int index);
