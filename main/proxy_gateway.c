#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/inet_chksum.h"
#include "lwip/inet.h"
#include "lwip/ip.h"
#include "esp_log.h"

#include <string.h>
#include "nvs.h"
#include "proxy_gateway.h"
#include "config.h"

#define NVS_NAMESPACE   "satura"
#define NVS_KEY_PX_EN   "px_en"
#define NVS_KEY_PX_HOST "px_host"
#define NVS_KEY_PX_PORT "px_port"

static uint32_t chksum_add_bytes(uint32_t sum, const uint8_t *buf, int len) {
    int i;
    for (i = 0; i + 1 < len; i += 2) {
        sum += ((uint32_t)buf[i] << 8) | buf[i + 1];
    }
    if (len & 1) {
        sum += (uint32_t)buf[len - 1] << 8;
    }
    return sum;
}

static uint16_t tcp_checksum_manual(const ip4_addr_t *src, const ip4_addr_t *dst,
                                     const uint8_t *tcp_segment, uint16_t tcp_len) {
    uint32_t sum = 0;
    uint8_t pseudo[12];

    memcpy(pseudo, &src->addr, 4);
    memcpy(pseudo + 4, &dst->addr, 4);
    pseudo[8]  = 0;
    pseudo[9]  = IP_PROTO_TCP;
    pseudo[10] = (uint8_t)(tcp_len >> 8);
    pseudo[11] = (uint8_t)(tcp_len & 0xFF);

    sum = chksum_add_bytes(sum, pseudo, sizeof(pseudo));
    sum = chksum_add_bytes(sum, tcp_segment, tcp_len);

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

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

static const char *TAG = "proxy_gw";

static proxy_gateway_config_t cached_cfg;
static bool cache_loaded = false;

static void ensure_cache_loaded(void) {
    if (!cache_loaded) {
        proxy_gateway_load(&cached_cfg);
        cache_loaded = true;
    }
}

/* Called by proxy_gateway_save() indirectly is not enough - the web
 * handler must invalidate the cache after saving. Exposed for that. */
void proxy_gateway_invalidate_cache(void) {
    cache_loaded = false;
}

#define ETH_HDR_LEN 14
#define ETHERTYPE_IPV4 0x0800

void proxy_gateway_maybe_redirect(struct pbuf *p) {
    ensure_cache_loaded();

    if (!cached_cfg.enabled || cached_cfg.host[0] == '\0' || cached_cfg.port == 0) return;
    if (p->len < ETH_HDR_LEN + sizeof(struct ip_hdr)) return;

    uint8_t *raw = (uint8_t *)p->payload;
    uint16_t ethertype = (raw[12] << 8) | raw[13];
    if (ethertype != ETHERTYPE_IPV4) return;

    struct ip_hdr *iph = (struct ip_hdr *)(raw + ETH_HDR_LEN);
    if (IPH_PROTO(iph) != IP_PROTO_TCP) return;

    /* Never redirect traffic destined for the bridge itself — this
        * would break the captive portal / web UI (also served on port 80). */
    if (ip4_addr1(&iph->dest) == GW_IP0 &&
        ip4_addr2(&iph->dest) == GW_IP1 &&
        ip4_addr3(&iph->dest) == GW_IP2 &&
        ip4_addr4(&iph->dest) == GW_IP3) {
        return;
    }

    uint8_t ihl = IPH_HL(iph) * 4;
    if (p->len < (u16_t)(ETH_HDR_LEN + ihl + sizeof(struct tcp_hdr))) return;

    struct tcp_hdr *tcph = (struct tcp_hdr *)((uint8_t *)iph + ihl);
    if (lwip_ntohs(tcph->dest) != 80) return;
    if (!(TCPH_FLAGS(tcph) & TCP_SYN) || (TCPH_FLAGS(tcph) & TCP_ACK)) return;



    ip4_addr_t gw_addr;
    if (!ip4addr_aton(cached_cfg.host, &gw_addr)) {
        ESP_LOGW(TAG, "invalid gateway host: %s", cached_cfg.host);
        return;
    }

    ip4_addr_set_u32(&iph->dest, gw_addr.addr);
    tcph->dest = lwip_htons(cached_cfg.port);



    IPH_CHKSUM_SET(iph, 0);
    IPH_CHKSUM_SET(iph, inet_chksum(iph, ihl));

    ip4_addr_t src_addr, dst_addr;
    src_addr.addr = iph->src.addr;
    dst_addr.addr = iph->dest.addr;

    if (p->next != NULL) {
        ESP_LOGW(TAG, "packet spans multiple pbufs, skipping redirect (unsupported)");
        return;
    }


    uint16_t tcp_seg_len = p->tot_len - ETH_HDR_LEN - ihl;

        tcph->chksum = 0;
        tcph->chksum = lwip_htons(
            tcp_checksum_manual(&src_addr, &dst_addr, (uint8_t *)tcph, tcp_seg_len)
        );

    ESP_LOGI(TAG, "redirected outgoing HTTP SYN to gateway %s:%d",
             cached_cfg.host, cached_cfg.port);
}
