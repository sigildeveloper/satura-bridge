#pragma once

typedef enum {
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_BT_CONNECTED,
    EVENT_BT_DISCONNECTED,

    /* Transport-agnostic role events — published by whichever concrete
     * link_iface_t (see link_iface.h) currently fills that role, in
     * addition to its own specific event above. nat_bridge subscribes
     * to these instead of BT/WiFi directly, so adding a new downlink
     * or uplink transport never requires touching nat_bridge.c. */
    EVENT_DOWNLINK_UP,
    EVENT_DOWNLINK_DOWN,
    EVENT_UPLINK_UP,
    EVENT_UPLINK_DOWN,
} event_type_t;

typedef void (*event_bus_handler_t)(event_type_t type);

void event_bus_subscribe(event_type_t type, event_bus_handler_t handler);
void event_bus_publish(event_type_t type);
