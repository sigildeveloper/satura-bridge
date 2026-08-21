#include <string.h>
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "esp_log.h"

#include "storage.h"
#include "proxy_gateway.h"

#define NVS_KEY_PX_EN   "px_en"
#define NVS_KEY_PX_HOST "px_host"
#define NVS_KEY_PX_PORT "px_port"

void proxy_gateway_load(proxy_gateway_config_t *out) {
    memset(out, 0, sizeof(*out));

    uint8_t enabled = 0;
    storage_get_u8(NVS_KEY_PX_EN, &enabled);
    out->enabled = enabled;

    storage_get_str(NVS_KEY_PX_HOST, out->host, sizeof(out->host));

    uint16_t port = 0;
    storage_get_u16(NVS_KEY_PX_PORT, &port);
    out->port = port;
}

bool proxy_gateway_save(const proxy_gateway_config_t *cfg) {
    bool ok1 = storage_set_u8(NVS_KEY_PX_EN, cfg->enabled ? 1 : 0);
    bool ok2 = storage_set_str(NVS_KEY_PX_HOST, cfg->host);
    bool ok3 = storage_set_u16(NVS_KEY_PX_PORT, cfg->port);
    return ok1 && ok2 && ok3;
}

static proxy_gateway_config_t cached_cfg;
static bool cache_loaded = false;

static const char *TAG = "proxy_gw";

/* Resolves cached_cfg.host (which may be a hostname or a plain IP)
 * into cached_cfg.resolved_ip once, so we don't hit DNS on every
 * single proxied request. */
static void resolve_upstream_host(void) {
    cached_cfg.resolved_ip[0] = '\0';
    if (cached_cfg.host[0] == '\0') return;

    struct in_addr direct;
    if (inet_pton(AF_INET, cached_cfg.host, &direct) == 1) {
        /* Already a plain IP — no resolution needed. */
        strncpy(cached_cfg.resolved_ip, cached_cfg.host, sizeof(cached_cfg.resolved_ip) - 1);
        return;
    }

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    int err = getaddrinfo(cached_cfg.host, NULL, &hints, &res);
    if (err != 0 || !res) {
        ESP_LOGW(TAG, "failed to resolve proxy gateway host '%s': %d", cached_cfg.host, err);
        return;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &addr->sin_addr, cached_cfg.resolved_ip, sizeof(cached_cfg.resolved_ip));
    freeaddrinfo(res);

    ESP_LOGI(TAG, "resolved proxy gateway '%s' -> %s", cached_cfg.host, cached_cfg.resolved_ip);
}

static void ensure_cache_loaded(void) {
    if (!cache_loaded) {
        proxy_gateway_load(&cached_cfg);
        resolve_upstream_host();
        cache_loaded = true;
    }
}

void proxy_gateway_invalidate_cache(void) {
    cache_loaded = false;
}

void proxy_gateway_get_upstream(char *host_out, size_t host_len, uint16_t *port_out) {
    ensure_cache_loaded();
    /* Prefer the already-resolved IP; if resolution failed for some
     * reason, fall back to the raw host string (harmless if it's
     * already numeric, a no-op connect failure otherwise). */
    const char *src = cached_cfg.resolved_ip[0] != '\0' ? cached_cfg.resolved_ip : cached_cfg.host;
    strncpy(host_out, src, host_len - 1);
    host_out[host_len - 1] = '\0';
    *port_out = cached_cfg.port;
}

bool proxy_gateway_is_enabled(void) {
    ensure_cache_loaded();
    return cached_cfg.enabled && cached_cfg.resolved_ip[0] != '\0' && cached_cfg.port != 0;
}
