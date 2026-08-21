#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool     enabled;
    char     host[64];
    uint16_t port;
    char     resolved_ip[16]; /* cached DNS resolution of host, not persisted */
} proxy_gateway_config_t;

void proxy_gateway_load(proxy_gateway_config_t *out);
bool proxy_gateway_save(const proxy_gateway_config_t *cfg);
void proxy_gateway_invalidate_cache(void);
void proxy_gateway_get_upstream(char *host_out, size_t host_len, uint16_t *port_out);
bool proxy_gateway_is_enabled(void);
