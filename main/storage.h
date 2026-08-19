#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Thin wrapper around NVS: opens/closes the handle and logs commit
 * failures in one place, so callers never touch nvs.h directly. */

bool storage_get_u8(const char *key, uint8_t *out);
bool storage_set_u8(const char *key, uint8_t value);

bool storage_get_u16(const char *key, uint16_t *out);
bool storage_set_u16(const char *key, uint16_t value);

bool storage_get_str(const char *key, char *out, size_t len);
bool storage_set_str(const char *key, const char *value);

bool storage_get_blob(const char *key, void *out, size_t len);
bool storage_set_blob(const char *key, const void *data, size_t len);

bool storage_erase_key(const char *key);