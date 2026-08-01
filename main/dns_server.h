#pragma once
#include <stdint.h>

void dns_server_start(void);
void dns_server_watchdog_tick(uint32_t now_ms);
