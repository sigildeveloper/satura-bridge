#pragma once
#include <stdbool.h>
#include "lwip/netif.h"

/* A "link" is anything that can bring up a network interface — BT PAN
 * today; BT PPP/RFCOMM, a UART/USB cellular modem, or Ethernet later.
 * Every link plays exactly one of two roles:
 *
 *   LINK_ROLE_DOWNLINK — faces the client device (the retro phone).
 *     The bridge is the gateway/DHCP server on this side. BT PAN is
 *     the only downlink today; BT PPP or a USB OTG Ethernet adapter
 *     for a wired retro device are future examples.
 *
 *   LINK_ROLE_UPLINK — faces the internet. The bridge is a client on
 *     this side (gets an address via DHCP/PPP/etc from someone else).
 *     WiFi STA is the only uplink today; a cellular modem or a wired
 *     Ethernet uplink are future examples.
 *
 * nat_bridge enables NAT between whichever downlink and uplink are
 * currently connected — it only ever talks to this interface, never
 * to a concrete transport, so it doesn't change when a new transport
 * is added on either side.
 *
 * get_netif() returns the raw lwIP struct netif*, not esp_netif_t* —
 * that's what ip_napt_enable_netif() (and most of lwIP's own API)
 * actually consumes, and it's the lowest common denominator: a
 * transport that only wraps a bare lwIP netif (like BT PAN via
 * bnep_lwip) can't produce an esp_netif_t* at all, while one that
 * does have an esp_netif_t* (like WiFi STA) can always unwrap it via
 * esp_netif_get_netif_impl(). */

typedef enum {
    LINK_ROLE_DOWNLINK,
    LINK_ROLE_UPLINK,
} link_role_t;

typedef struct link_iface {
    const char *name;        /* "bt_pan", "wifi_sta", ... — for logs only */
    link_role_t role;

    /* One-time setup at boot. May be NULL if the transport's own
     * *_init() is already called separately from app_bootstrap.c —
     * only implement this if registering is the ONLY init step needed. */
    void (*init)(void);

    bool (*is_connected)(void);

    /* NULL if not currently connected. Ownership stays with the
     * transport module — this is a borrowed pointer, never freed by
     * the caller. */
    struct netif *(*get_netif)(void);
} link_iface_t;

/* Called once per transport, typically from that transport's own
 * *_init(), before the link is expected to ever report connected. */
void link_registry_register(const link_iface_t *link);

/* First registered link in the given role that reports connected, or
 * NULL if none currently is. O(n) over a handful of entries — fine to
 * call from an event handler, not a hot path. */
const link_iface_t *link_registry_find_connected(link_role_t role);
