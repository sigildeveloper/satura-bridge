#include <string.h>
#include "nvs.h"
#include "proxy_gateway.h"

#define NVS_NAMESPACE   "satura"
#define NVS_KEY_PX_EN   "px_en"
#define NVS_KEY_PX_HOST "px_host"
#define NVS_KEY_PX_PORT "px_port"

void proxy_gateway_load(proxy_gateway_config_t *out) {
    memset(out, 0, sizeof(*out));

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    uint8_t enabled = 0;
    nvs_get_u8(h, NVS_KEY_PX_EN, &enabled);
    out->enabled = enabled;

    size_t host_len = sizeof(out->host);
    nvs_get_str(h, NVS_KEY_PX_HOST, out->host, &host_len);

    uint16_t port = 0;
    nvs_get_u16(h, NVS_KEY_PX_PORT, &port);
    out->port = port;

    nvs_close(h);
}

bool proxy_gateway_save(const proxy_gateway_config_t *cfg) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;

    nvs_set_u8(h, NVS_KEY_PX_EN, cfg->enabled ? 1 : 0);
    nvs_set_str(h, NVS_KEY_PX_HOST, cfg->host);
    nvs_set_u16(h, NVS_KEY_PX_PORT, cfg->port);

    nvs_commit(h);
    nvs_close(h);
    return true;
}

static proxy_gateway_config_t cached_cfg;
static bool cache_loaded = false;

static void ensure_cache_loaded(void) {
    if (!cache_loaded) {
        proxy_gateway_load(&cached_cfg);
        cache_loaded = true;
    }
}

void proxy_gateway_invalidate_cache(void) {
    cache_loaded = false;
}

void proxy_gateway_get_upstream(char *host_out, size_t host_len, uint16_t *port_out) {
    ensure_cache_loaded();
    strncpy(host_out, cached_cfg.host, host_len - 1);
    host_out[host_len - 1] = '\0';
    *port_out = cached_cfg.port;
}

bool proxy_gateway_is_enabled(void) {
    ensure_cache_loaded();
    return cached_cfg.enabled && cached_cfg.host[0] != '\0' && cached_cfg.port != 0;
}
