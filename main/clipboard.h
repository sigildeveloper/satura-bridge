#pragma once
#include "esp_http_server.h"

/* Shared clipboard/pasteboard, reachable from both the phone (over BT
 * PAN, same as the rest of the web UI) and any device on the WiFi
 * uplink's LAN. Meant for pasting things that are painful to type on
 * a T9 keypad — long URLs, WEP keys, game unlock codes — from a real
 * keyboard, then reading or tapping them on the phone.
 *
 * Ring buffer: newest entries push out the oldest once CLIP_RING_MAX
 * is reached. Favorites: pinned out of the ring, capped separately,
 * never auto-evicted. Both persist across reboot in one NVS blob. */

#define CLIP_RING_MAX  15
#define CLIP_FAV_MAX   8
#define CLIP_TEXT_MAX  256

void clipboard_init(void);

esp_err_t handler_clip_get(httpd_req_t *req);
esp_err_t handler_clip_add_post(httpd_req_t *req);
esp_err_t handler_clip_action_get(httpd_req_t *req); /* /clip/action?op=pin|unpin|delete&id=N */
