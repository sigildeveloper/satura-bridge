#include "esp_timer.h"
#include "uptime.h"

static int64_t boot_us = 0;

void uptime_init(void) {
    boot_us = esp_timer_get_time();
}

uint32_t uptime_seconds(void) {
    return (uint32_t)((esp_timer_get_time() - boot_us) / 1000000ULL);
}
