#pragma once

typedef enum {
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_BT_CONNECTED,
    EVENT_BT_DISCONNECTED,
} event_type_t;

typedef void (*event_bus_handler_t)(event_type_t type);

void event_bus_subscribe(event_type_t type, event_bus_handler_t handler);
void event_bus_publish(event_type_t type);
