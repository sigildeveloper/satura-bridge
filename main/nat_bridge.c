#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "esp_log.h"

#include "nat_bridge.h"
#include "link_iface.h"
#include "event_bus.h"

static const char *TAG = "nat_bridge";

static void update_nat_lwip_ctx(void *arg) {
    (void)arg;
    const link_iface_t *down = link_registry_find_connected(LINK_ROLE_DOWNLINK);
    if (!down || !down->get_netif) return;

    struct netif *netif = down->get_netif();
    if (!netif) {
        ESP_LOGW(TAG, "downlink '%s' reports connected but has no netif yet",
                 down->name);
        return;
    }

    const link_iface_t *up = link_registry_find_connected(LINK_ROLE_UPLINK);
    ip_napt_enable_netif(netif, up ? 1 : 0);
    if (up) {
        ESP_LOGI(TAG, "NAT enabled: %s -> %s", down->name, up->name);
    } else {
        ESP_LOGI(TAG, "NAT disabled: %s has no uplink", down->name);
    }
}

void nat_bridge_update(void) {
    tcpip_callback(update_nat_lwip_ctx, NULL);
}

static void on_bridge_event(event_type_t type) {
    (void)type;
    nat_bridge_update();
}

void nat_bridge_subscribe(void) {
    /* Transport-agnostic: any link_iface_t (see link_iface.h) that
     * publishes these when its connection state changes triggers a
     * NAT re-evaluation, whether it's BT PAN, WiFi, or something
     * added later — this file never needs to change again to support
     * a new downlink or uplink transport. */
    event_bus_subscribe(EVENT_DOWNLINK_UP, on_bridge_event);
    event_bus_subscribe(EVENT_DOWNLINK_DOWN, on_bridge_event);
    event_bus_subscribe(EVENT_UPLINK_UP, on_bridge_event);
    event_bus_subscribe(EVENT_UPLINK_DOWN, on_bridge_event);
}
