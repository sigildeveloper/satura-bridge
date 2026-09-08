#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_http_server.h"

void html_escape(const char *src, char *dst, size_t n);
void set_no_cache(httpd_req_t *req, const char *type);

/* Raw HTTP/1.0 streaming response helpers — see http_utils.c for why
 * these exist instead of httpd_resp_send_chunk(). Typical use:
 *   int sock = http_raw_response_start(req, "text/html");
 *   if (sock < 0) return ESP_FAIL;
 *   ... build small pieces into a stack buffer, http_raw_send(sock, buf, n) ...
 *   http_raw_response_end(req, sock);
 *   return ESP_OK;
 */
int  http_raw_response_start(httpd_req_t *req, const char *content_type);
bool http_raw_send(int sockfd, const char *data, size_t len);
void http_raw_response_end(httpd_req_t *req, int sockfd);

/* True if host_no_port (a Host header with any :port stripped) refers
 * to the bridge itself — its gateway IP or its current STA IP. */
bool host_matches_bridge(const char *host_no_port);

/* True if this request's Host header refers to something other than
 * the bridge itself — i.e. it's real internet traffic that arrived
 * here because proxy-mode DNS pointed everything at us. */
bool is_foreign_host(httpd_req_t *req);

void url_encode(const char *src, char *dst, size_t dst_len);
void url_decode(const char *src, char *dst, size_t dst_len);
