#include <stddef.h>
#include "esp_log.h"
#include "event_bus.h"

static const char *TAG = "event_bus";

#define EVENT_TYPE_COUNT     8
#define MAX_HANDLERS_PER_TYPE 4

static event_bus_handler_t handlers[EVENT_TYPE_COUNT][MAX_HANDLERS_PER_TYPE];
static int handler_count[EVENT_TYPE_COUNT];

void event_bus_subscribe(event_type_t type, event_bus_handler_t handler) {
    if (type < 0 || type >= EVENT_TYPE_COUNT || !handler) return;
    if (handler_count[type] >= MAX_HANDLERS_PER_TYPE) {
        ESP_LOGW(TAG, "no room for another handler on event %d", (int)type);
        return;
    }
    handlers[type][handler_count[type]++] = handler;
}

void event_bus_publish(event_type_t type) {
    if (type < 0 || type >= EVENT_TYPE_COUNT) return;
    for (int i = 0; i < handler_count[type]; i++) {
        handlers[type][i](type);
    }
}
