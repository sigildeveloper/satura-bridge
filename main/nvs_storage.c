#include <string.h>
#include "storage.h"
#include "nvs_storage.h"

#define NVS_KEY_NET_COUNT  "net_count"
#define NVS_KEY_NET_BLOB   "net_blob"

/* ---- Multi-network API ---- */

int nvs_storage_load_networks(wifi_network_t *out, int max_count) {
    uint8_t count = 0;
    if (!storage_get_u8(NVS_KEY_NET_COUNT, &count)) return 0;
    if (count > WIFI_MAX_SAVED_NETWORKS) count = WIFI_MAX_SAVED_NETWORKS;

    /* Always read into a full-size local buffer, regardless of what the
     * caller asked for — this decouples the stored blob size (based on
     * stored count) from the caller's buffer size (max_count), which
     * previously allowed a stack buffer overflow when max_count < count. */
    wifi_network_t full[WIFI_MAX_SAVED_NETWORKS];
    if (count > 0) {
        if (!storage_get_blob(NVS_KEY_NET_BLOB, full, sizeof(wifi_network_t) * count)) {
            return 0;
        }
    }

    int result_count = count;
    if (result_count > max_count) result_count = max_count;
    if (result_count > 0) {
        memcpy(out, full, sizeof(wifi_network_t) * result_count);
    }
    return result_count;
}

bool nvs_storage_save_networks(const wifi_network_t *networks, int count) {
    if (count < 0) count = 0;
    if (count > WIFI_MAX_SAVED_NETWORKS) count = WIFI_MAX_SAVED_NETWORKS;

    bool ok = storage_set_u8(NVS_KEY_NET_COUNT, (uint8_t)count);
    if (count > 0) {
        ok = storage_set_blob(NVS_KEY_NET_BLOB, networks, sizeof(wifi_network_t) * count) && ok;
    } else {
        storage_erase_key(NVS_KEY_NET_BLOB);
    }
    return ok;
}

bool nvs_storage_add_network(const char *ssid, const char *pass, bool hidden) {
    wifi_network_t list[WIFI_MAX_SAVED_NETWORKS];
    int count = nvs_storage_load_networks(list, WIFI_MAX_SAVED_NETWORKS);

    /* Replace if SSID already exists */
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].ssid, ssid) == 0) {
            strncpy(list[i].pass, pass, sizeof(list[i].pass) - 1);
            list[i].pass[sizeof(list[i].pass) - 1] = '\0';
            list[i].hidden = hidden;
            return nvs_storage_save_networks(list, count);
        }
    }

    if (count >= WIFI_MAX_SAVED_NETWORKS) return false; /* list full */

    strncpy(list[count].ssid, ssid, sizeof(list[count].ssid) - 1);
    list[count].ssid[sizeof(list[count].ssid) - 1] = '\0';
    strncpy(list[count].pass, pass, sizeof(list[count].pass) - 1);
    list[count].pass[sizeof(list[count].pass) - 1] = '\0';
    list[count].hidden = hidden;
    count++;

    return nvs_storage_save_networks(list, count);
}

bool nvs_storage_remove_network(int index) {
    wifi_network_t list[WIFI_MAX_SAVED_NETWORKS];
    int count = nvs_storage_load_networks(list, WIFI_MAX_SAVED_NETWORKS);

    if (index < 0 || index >= count) return false;

    for (int i = index; i < count - 1; i++) {
        list[i] = list[i + 1];
    }
    count--;

    return nvs_storage_save_networks(list, count);
}

void nvs_storage_clear_networks(void) {
    storage_erase_key(NVS_KEY_NET_COUNT);
    storage_erase_key(NVS_KEY_NET_BLOB);
}
