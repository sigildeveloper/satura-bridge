#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcpip.h"
#include "esp_log.h"

#include "nat_bridge.h"
#include "config.h"
#include "app_state.h"
#include "event_bus.h"

static const char *TAG = "nat_bridge";
static struct netif *bt_netif = NULL;

static void find_bt_netif(void) {
    struct netif *p = netif_list;
    while (p) {
        const ip4_addr_t *ip = netif_ip4_addr(p);
        if (ip4_addr1(ip) == GW_IP0 &&
            ip4_addr2(ip) == GW_IP1 &&
            ip4_addr3(ip) == GW_IP2) {
            bt_netif = p;
            return;
        }
        p = p->next;
    }
}

static void update_nat_lwip_ctx(void *arg) {
    (void)arg;
    if (!bt_netif) find_bt_netif();
    if (!bt_netif) {
        struct netif *p = netif_list;
        while (p) {
            ESP_LOGW(TAG, "  netif: %d.%d.%d.%d",
                ip4_addr1(netif_ip4_addr(p)), ip4_addr2(netif_ip4_addr(p)),
                ip4_addr3(netif_ip4_addr(p)), ip4_addr4(netif_ip4_addr(p)));
            p = p->next;
        }
        return;
    }
    bool enable = get_bt_connected() && get_wifi_connected();
    ip_napt_enable_netif(bt_netif, enable ? 1 : 0);
}

void nat_bridge_update(void) {
    tcpip_callback(update_nat_lwip_ctx, NULL);
}

static void on_bridge_event(event_type_t type) {
    (void)type;
    nat_bridge_update();
}

void nat_bridge_subscribe(void) {
    event_bus_subscribe(EVENT_WIFI_CONNECTED, on_bridge_event);
    event_bus_subscribe(EVENT_WIFI_DISCONNECTED, on_bridge_event);
    event_bus_subscribe(EVENT_BT_CONNECTED, on_bridge_event);
    event_bus_subscribe(EVENT_BT_DISCONNECTED, on_bridge_event);
}
