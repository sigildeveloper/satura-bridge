#include "nvs.h"
#include "esp_log.h"
#include "storage.h"

#define STORAGE_NAMESPACE "satura"

static const char *TAG = "storage";

static bool commit_and_close(nvs_handle_t h) {
    esp_err_t err = nvs_commit(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(h);
    return err == ESP_OK;
}

bool storage_get_u8(const char *key, uint8_t *out) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t err = nvs_get_u8(h, key, out);
    nvs_close(h);
    return err == ESP_OK;
}

bool storage_set_u8(const char *key, uint8_t value) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_u8(h, key, value);
    return commit_and_close(h);
}

bool storage_get_u16(const char *key, uint16_t *out) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t err = nvs_get_u16(h, key, out);
    nvs_close(h);
    return err == ESP_OK;
}

bool storage_set_u16(const char *key, uint16_t value) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_u16(h, key, value);
    return commit_and_close(h);
}

bool storage_get_str(const char *key, char *out, size_t len) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t actual_len = len;
    esp_err_t err = nvs_get_str(h, key, out, &actual_len);
    nvs_close(h);
    return err == ESP_OK;
}

bool storage_set_str(const char *key, const char *value) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_str(h, key, value);
    return commit_and_close(h);
}

bool storage_get_blob(const char *key, void *out, size_t len) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t actual_len = len;
    esp_err_t err = nvs_get_blob(h, key, out, &actual_len);
    nvs_close(h);
    return err == ESP_OK;
}

bool storage_set_blob(const char *key, const void *data, size_t len) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_blob(h, key, data, len);
    return commit_and_close(h);
}

bool storage_erase_key(const char *key) {
    nvs_handle_t h;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_erase_key(h, key);
    return commit_and_close(h);
}