#pragma once
#include <stdbool.h>

/* Board hardware abstraction. Exactly one backend is compiled in —
 * board_generic.c, board_core2.c, or board_stickc_plus2.c — selected
 * by Kconfig (menu "Satura Bridge" -> "Hardware board", see
 * main/Kconfig) and wired up in main/CMakeLists.txt. Application code
 * (wifi_manager, bt_pan, nat_bridge, http_routes, ...) calls only
 * this interface and never touches AXP192 registers, GPIO numbers, or
 * ADC channels directly — those live entirely inside board_*.c. */

/* One-time setup at boot. Safe/no-op on boards with nothing to set
 * up. Already calls board_power_hold() internally where that
 * matters — app_bootstrap.c also calls board_power_hold() on its own,
 * separately and earlier, only because that specific call needs to
 * happen before anything else at all on boards where power cuts out
 * otherwise (see the comment at that call site). */
void board_init(void);

/* Keeps the board powered on boards that need an explicit hold signal
 * (M5StickC Plus2's GPIO4) — a no-op everywhere else. Idempotent;
 * safe to call more than once. */
void board_power_hold(void);

const char *board_get_name(void);

/* 0-100, or -1 if unavailable on this board (no battery, read failed). */
int board_get_battery_percent(void);

/* Millivolts, or -1 if unavailable. */
int board_get_battery_voltage_mv(void);

/* False if unavailable/unsupported on this board, not just "not
 * currently charging" — callers can't tell the difference from the
 * return value alone, same convention as the percent/voltage getters. */
bool board_battery_is_charging(void);

bool board_battery_charging_status_available(void);
