#include "battery.h"
#include "board/board.h"

/* board_init() is called separately, early, from app_bootstrap.c's
 * btstack_main() — deliberately NOT triggered here, so that "call
 * battery_init()" never has the surprising side effect of also
 * initializing unrelated board hardware (power hold, etc.) depending
 * on init order. Kept as an explicit no-op rather than removed, so
 * existing callers don't need to change. */
void battery_init(void) {}

int battery_get_percent(void) {
    return board_get_battery_percent();
}

int battery_get_voltage_mv(void) {
    return board_get_battery_voltage_mv();
}

bool battery_is_charging(void) {
    return board_battery_is_charging();
}

bool battery_charging_status_available(void) {
    return board_battery_charging_status_available();
}
