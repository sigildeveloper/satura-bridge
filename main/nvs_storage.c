#include <string.h>
#include "nvs.h"
#include "nvs_storage.h"

#define NVS_NAMESPACE       "satura"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASS        "pass"
#define NVS_KEY_NET_COUNT   "net_count"
#define NVS_KEY_NET_BLOB    "net_blob"

/* ---- Legacy single-network API (unused after migration, kept for safety) ---- */

bool nvs_storage_save(const char *ssid, const char *pass) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_str(h, NVS_KEY_SSID, ssid);
    nvs_set_str(h, NVS_KEY_PASS, pass);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool nvs_storage_load(char *ssid_out, size_t ssid_len,
                       char *pass_out, size_t pass_len) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sl = ssid_len, pl = pass_len;
    bool ok = nvs_get_str(h, NVS_KEY_SSID, ssid_out, &sl) == ESP_OK
           && nvs_get_str(h, NVS_KEY_PASS, pass_out, &pl) == ESP_OK
           && strlen(ssid_out) > 0;
    nvs_close(h);
    return ok;
}

void nvs_storage_clear(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, NVS_KEY_SSID);
        nvs_erase_key(h, NVS_KEY_PASS);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* ---- Multi-network API ---- */

int nvs_storage_load_networks(wifi_network_t *out, int max_count) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return 0;

    uint8_t count = 0;
    if (nvs_get_u8(h, NVS_KEY_NET_COUNT, &count) != ESP_OK) {
        nvs_close(h);
        return 0;
    }
    if (count > WIFI_MAX_SAVED_NETWORKS) count = WIFI_MAX_SAVED_NETWORKS;

    /* Always read into a full-size local buffer, regardless of what the
     * caller asked for — this decouples the NVS blob size (based on
     * stored count) from the caller's buffer size (max_count), which
     * previously allowed a stack buffer overflow when max_count < count. */
    wifi_network_t full[WIFI_MAX_SAVED_NETWORKS];
    size_t blob_len = sizeof(wifi_network_t) * count;
    if (blob_len > 0) {
        size_t actual_len = blob_len;
        if (nvs_get_blob(h, NVS_KEY_NET_BLOB, full, &actual_len) != ESP_OK) {
            nvs_close(h);
            return 0;
        }
    }
    nvs_close(h);

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

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;

    nvs_set_u8(h, NVS_KEY_NET_COUNT, (uint8_t)count);
    if (count > 0) {
        nvs_set_blob(h, NVS_KEY_NET_BLOB, networks, sizeof(wifi_network_t) * count);
    } else {
        nvs_erase_key(h, NVS_KEY_NET_BLOB);
    }
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool nvs_storage_add_network(const char *ssid, const char *pass) {
    wifi_network_t list[WIFI_MAX_SAVED_NETWORKS];
    int count = nvs_storage_load_networks(list, WIFI_MAX_SAVED_NETWORKS);

    /* Replace if SSID already exists */
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].ssid, ssid) == 0) {
            strncpy(list[i].pass, pass, sizeof(list[i].pass) - 1);
            list[i].pass[sizeof(list[i].pass) - 1] = '\0';
            return nvs_storage_save_networks(list, count);
        }
    }

    if (count >= WIFI_MAX_SAVED_NETWORKS) return false; /* list full */

    strncpy(list[count].ssid, ssid, sizeof(list[count].ssid) - 1);
    list[count].ssid[sizeof(list[count].ssid) - 1] = '\0';
    strncpy(list[count].pass, pass, sizeof(list[count].pass) - 1);
    list[count].pass[sizeof(list[count].pass) - 1] = '\0';
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
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, NVS_KEY_NET_COUNT);
        nvs_erase_key(h, NVS_KEY_NET_BLOB);
        nvs_commit(h);
        nvs_close(h);
    }
}
