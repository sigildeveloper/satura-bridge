#include <string.h>
#include "esp_log.h"

#include "link_iface.h"

static const char *TAG = "link_registry";

#define MAX_LINKS 4

static const link_iface_t *links[MAX_LINKS];
static int link_count = 0;

void link_registry_register(const link_iface_t *link) {
    if (!link || link_count >= MAX_LINKS) {
        ESP_LOGE(TAG, "cannot register link %s (count=%d, max=%d)",
                 link ? link->name : "(null)", link_count, MAX_LINKS);
        return;
    }
    links[link_count++] = link;
    ESP_LOGI(TAG, "registered %s link: %s",
             link->role == LINK_ROLE_DOWNLINK ? "downlink" : "uplink",
             link->name);
}

const link_iface_t *link_registry_find_connected(link_role_t role) {
    for (int i = 0; i < link_count; i++) {
        if (links[i]->role == role && links[i]->is_connected &&
            links[i]->is_connected()) {
            return links[i];
        }
    }
    return NULL;
}
