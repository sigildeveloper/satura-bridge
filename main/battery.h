#pragma once
#include <stdbool.h>

/* Application-level wrapper over the board hardware abstraction
 * (main/board/board.h). Kept as a separate module — rather than
 * having every caller include board.h directly — so existing callers
 * (http_routes.c) don't need to change, and so the app layer never
 * has to know which concrete board backend is compiled in. */

void battery_init(void);

/* 0-100, or -1 if unavailable on this board or the read failed.
 * Callers should treat -1 as "don't show a battery line at all", not
 * as 0%. */
int battery_get_percent(void);

/* Millivolts, or -1 if unavailable. */
int battery_get_voltage_mv(void);

/* False if unavailable/unsupported on this board, or not currently
 * charging. */
bool battery_is_charging(void);


bool battery_charging_status_available(void);
