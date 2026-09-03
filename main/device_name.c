#include <string.h>
#include <stdio.h>

#include "esp_mac.h"

#include "device_name.h"
#include "storage.h"

#define NVS_KEY_NAME "device_name"
#define DEFAULT_PREFIX "Satura Bridge "

static char g_name[DEVICE_NAME_MAX + 1] = {0};
static bool g_loaded = false;

static void generate_default(char *out, size_t out_len) {
    uint8_t mac[6] = {0};
    /* ESP32's Bluetooth MAC is a fixed eFuse value, readable at any
     * time regardless of whether the BT controller has been powered
     * on yet — unlike gap_local_bd_addr(), which needs HCI already up.
     * That matters here: this runs before bt_pan_init(), long before
     * hci_power_control(HCI_POWER_ON). */
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(out, out_len, "%s%02X%02X", DEFAULT_PREFIX, mac[4], mac[5]);
}

void device_name_init(void) {
    if (g_loaded) return;

    if (!storage_get_str(NVS_KEY_NAME, g_name, sizeof(g_name)) ||
        g_name[0] == '\0') {
        generate_default(g_name, sizeof(g_name));
        storage_set_str(NVS_KEY_NAME, g_name);
    }
    g_loaded = true;
}

const char *device_name_get(void) {
    device_name_init(); /* safe no-op if already loaded — same lazy
                          * pattern as proxy_gateway/nvs_storage */
    return g_name;
}

bool device_name_set(const char *name) {
    if (!name || !name[0]) return false;
    if (strlen(name) >= sizeof(g_name)) return false;

    if (!storage_set_str(NVS_KEY_NAME, name)) return false;

    strncpy(g_name, name, sizeof(g_name) - 1);
    g_name[sizeof(g_name) - 1] = '\0';
    g_loaded = true;
    return true;
}
