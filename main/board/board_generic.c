#include "board.h"

/* Plain ESP32 dev board: no PMIC, no battery, no board-specific power
 * sequencing. This is the default backend — every function here is a
 * safe no-op/unavailable-report, never touching any GPIO or bus. */

void board_init(void) {}
void board_power_hold(void) {}
const char *board_get_name(void) { return "Generic ESP32"; }
int board_get_battery_percent(void) { return -1; }
int board_get_battery_voltage_mv(void) { return -1; }
bool board_battery_is_charging(void) { return false; }

bool board_battery_charging_status_available(void) {
    return false;
}
