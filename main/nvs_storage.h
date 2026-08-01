#pragma once
#include <stdbool.h>
#include <stddef.h>

bool nvs_storage_save(const char *ssid, const char *pass);
bool nvs_storage_load(char *ssid_out, size_t ssid_len,
                       char *pass_out, size_t pass_len);
void nvs_storage_clear(void);
