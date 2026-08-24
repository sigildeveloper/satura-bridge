#pragma once
#include "esp_http_server.h"

/* Reached via the wildcard "\*" route (and directly from handler_root
 * when proxy mode is enabled and the Host header is foreign). Opens a
 * raw TCP connection to the configured upstream gateway, replays the
 * client's request onto it, and relays the response back chunk by
 * chunk. */
esp_err_t handler_proxy_relay(httpd_req_t *req);
