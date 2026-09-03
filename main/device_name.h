#pragma once
#include <stdbool.h>

#define DEVICE_NAME_MAX 32

/* Loads the saved name from NVS. If none is saved yet (first boot),
 * generates a default of "Satura Bridge XXXX" — XXXX being the last
 * two bytes of the device's Bluetooth MAC in hex — so that several
 * bridges nearby don't show up as identical, indistinguishable
 * entries in a phone's device picker. Must run before bt_pan_init(),
 * which reads the result via device_name_get(). */
void device_name_init(void);

/* Always returns a valid, non-empty, NUL-terminated string owned by
 * this module — never free it. */
const char *device_name_get(void);

/* Validates (non-empty, fits DEVICE_NAME_MAX) and persists. Takes
 * effect on the Bluetooth side only after a reboot — bt_pan_init()
 * reads the name once at boot, before the radio powers on. */
bool device_name_set(const char *name);
