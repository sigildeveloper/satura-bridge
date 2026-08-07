#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIFI_MAX_SAVED_NETWORKS 6

typedef struct {
    char ssid[33];
    char pass[64];
} wifi_network_t;

/* Legacy single-network API — kept for compatibility during migration */
bool nvs_storage_save(const char *ssid, const char *pass);
bool nvs_storage_load(char *ssid_out, size_t ssid_len,
                       char *pass_out, size_t pass_len);
void nvs_storage_clear(void);

/* Multi-network API */
int  nvs_storage_load_networks(wifi_network_t *out, int max_count);
bool nvs_storage_save_networks(const wifi_network_t *networks, int count);
bool nvs_storage_add_network(const char *ssid, const char *pass);
bool nvs_storage_remove_network(int index);
void nvs_storage_clear_networks(void);