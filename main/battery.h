#pragma once
#include <stdbool.h>

/* Sets up the I2C bus to the AXP192, if ENABLE_M5STACK_BATTERY is set
 * in config.h. A no-op otherwise — safe to call unconditionally. */
void battery_init(void);

/* 0-100, or -1 if the feature is disabled or the read failed (I2C
 * error, chip not present). Callers should treat -1 as "don't show
 * a battery line at all", not as 0%. */
int battery_get_percent(void);

/* False if disabled, unavailable, or not currently charging. */
bool battery_is_charging(void);
