#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

#include "proxy_gateway.h"
#include "storage.h"

static const char *TAG = "proxy_gateway";

#define KEY_ENABLED "proxy_enabled"
#define KEY_HOST    "proxy_host"
#define KEY_PORT    "proxy_port"

#define DEFAULT_PORT 80

static proxy_gateway_config_t g_cfg;
static bool g_loaded = false;

static void set_defaults(proxy_gateway_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = false;
    cfg->port = DEFAULT_PORT;
}

static void load_locked(void)
{
    if (g_loaded) {
        return;
    }

    set_defaults(&g_cfg);

    uint8_t enabled = 0;
    uint16_t port = DEFAULT_PORT;

    if (storage_get_u8(KEY_ENABLED, &enabled)) {
        g_cfg.enabled = enabled != 0;
    }

    if (storage_get_str(KEY_HOST, g_cfg.host, sizeof(g_cfg.host))) {
        g_cfg.host[sizeof(g_cfg.host) - 1] = '\0';
    }

    if (storage_get_u16(KEY_PORT, &port) && port != 0) {
        g_cfg.port = port;
    }

    g_loaded = true;
}

void proxy_gateway_load(proxy_gateway_config_t *out)
{
    if (!out) {
        return;
    }

    load_locked();
    *out = g_cfg;
}

bool proxy_gateway_save(const proxy_gateway_config_t *cfg)
{
    if (!cfg) {
        return false;
    }

    proxy_gateway_config_t normalized = *cfg;
    normalized.host[sizeof(normalized.host) - 1] = '\0';

    if (normalized.port == 0) {
        normalized.port = DEFAULT_PORT;
    }

    /*
     * The web UI allows an empty host while editing the form. Do not
     * enable the gateway with an unusable configuration.
     */
    if (normalized.enabled && normalized.host[0] == '\0') {
        ESP_LOGW(TAG, "refusing to enable proxy gateway with empty host");
        return false;
    }

    bool ok = true;
    ok = storage_set_u8(KEY_ENABLED, normalized.enabled ? 1 : 0) && ok;
    ok = storage_set_str(KEY_HOST, normalized.host) && ok;
    ok = storage_set_u16(KEY_PORT, normalized.port) && ok;

    if (ok) {
        g_cfg = normalized;
        g_cfg.resolved_ip[0] = '\0';
        g_loaded = true;
    }

    return ok;
}

void proxy_gateway_invalidate_cache(void)
{
    if (!g_loaded) {
        return;
    }

    g_cfg.resolved_ip[0] = '\0';
}

static bool resolve_ipv4(const char *host, char *ip_out, size_t ip_len)
{
    if (!host || !host[0] || !ip_out || ip_len == 0) {
        return false;
    }

    struct in_addr numeric;
    if (inet_pton(AF_INET, host, &numeric) == 1) {
        if (!inet_ntoa_r(numeric, ip_out, ip_len)) {
            ip_out[0] = '\0';
            return false;
        }
        return true;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    int err = getaddrinfo(host, NULL, &hints, &result);
    if (err != 0 || result == NULL) {
        ESP_LOGW(TAG, "DNS resolution failed for '%s': %d", host, err);
        return false;
    }

    bool ok = false;
    struct addrinfo *p = result;
    while (p) {
        if (p->ai_family == AF_INET && p->ai_addr &&
            p->ai_addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
            if (inet_ntoa_r(addr->sin_addr, ip_out, ip_len)) {
                ok = true;
                break;
            }
        }
        p = p->ai_next;
    }

    freeaddrinfo(result);
    return ok;
}

bool proxy_gateway_is_enabled(void)
{
    load_locked();
    return g_cfg.enabled && g_cfg.host[0] != '\0' && g_cfg.port != 0;
}

void proxy_gateway_get_upstream(char *host_out, size_t host_len,
                                uint16_t *port_out)
{
    if (host_out && host_len > 0) {
        host_out[0] = '\0';
    }
    if (port_out) {
        *port_out = 0;
    }

    load_locked();

    if (!g_cfg.host[0] || g_cfg.port == 0) {
        return;
    }

    /*
     * proxy_relay.c opens an AF_INET socket and therefore expects an
     * IPv4 address here, not a hostname. Resolve once and keep the
     * result in RAM until the gateway configuration is invalidated.
     */
    if (g_cfg.resolved_ip[0] == '\0') {
        if (!resolve_ipv4(g_cfg.host, g_cfg.resolved_ip,
                          sizeof(g_cfg.resolved_ip))) {
            return;
        }
        ESP_LOGI(TAG, "resolved %s -> %s", g_cfg.host, g_cfg.resolved_ip);
    }

    if (host_out && host_len > 0) {
        strncpy(host_out, g_cfg.resolved_ip, host_len - 1);
        host_out[host_len - 1] = '\0';
    }
    if (port_out) {
        *port_out = g_cfg.port;
    }
}
