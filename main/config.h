#pragma once

#define GW_IP0 192
#define GW_IP1 168
#define GW_IP2 7
#define GW_IP3 1
#define GW_IP_STR "192.168.7.1"

#define FALLBACK_DNS "8.8.8.8"

/* Hardware-specific, not for the general build: reads battery status
 * from the onboard AXP192 PMIC over I2C. Only meaningful on an
 * M5Stack Core2 (original — AXP192, not the v1.1 revision, which uses
 * AXP2101 instead and needs a different driver entirely). A plain
 * ESP32 dev board has no such chip; leave this at 0 unless you know
 * you're on the right hardware. */
#define ENABLE_M5STACK_BATTERY 1
