#include <string.h>
#include "nvs.h"
#include "nvs_storage.h"

#define NVS_NAMESPACE "satura"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "pass"

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
