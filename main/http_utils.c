#include <string.h>
#include "lwip/sockets.h"
#include "http_utils.h"
#include "config.h"
#include "wifi_manager.h"

void html_escape(const char *src, char *dst, size_t n) {
    size_t w = 0;
    for (const char *p = src; *p && w + 7 < n; p++) {
        const char *rep = NULL;
        if      (*p == '&') rep = "&amp;";
        else if (*p == '<') rep = "&lt;";
        else if (*p == '>') rep = "&gt;";
        else if (*p == '"') rep = "&quot;";
        if (rep) { size_t l = strlen(rep); memcpy(dst + w, rep, l); w += l; }
        else dst[w++] = *p;
    }
    dst[w] = 0;
}

void set_no_cache(httpd_req_t *req, const char *type) {
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-cache, no-store, must-revalidate");
}

/*
 * Streams a page directly to the client socket in small pieces instead of
 * rendering it into one big malloc'd buffer first (the pattern that used
 * to allocate 3-8 KB per request in handler_clip_get/handler_networks_get/
 * handler_status — a single contiguous allocation that size can fail
 * under DRAM fragmentation with WiFi+BT both active, where free heap is
 * commonly only 35-65 KB).
 *
 * This intentionally does NOT use httpd_resp_send_chunk(): that always
 * frames the body as HTTP/1.1 "Transfer-Encoding: chunked", which is
 * exactly what makes the WAP/J2ME-era target browsers ECONNRESET (see the
 * comment on relay_bytes_httpd() in proxy_relay.c). Instead this writes a
 * plain HTTP/1.0 header with "Connection: close" straight to the socket —
 * old browsers already know that means "read until the socket closes" —
 * and the caller sends body pieces the same way with http_raw_send().
 */
int http_raw_response_start(httpd_req_t *req, const char *content_type) {
    int sock = httpd_req_to_sockfd(req);
    if (sock < 0) return -1;

    char hdr[192];
    int len = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Connection: close\r\n\r\n",
        content_type);
    if (len < 0) return -1;
    if (len >= (int)sizeof(hdr)) len = (int)sizeof(hdr) - 1;

    if (send(sock, hdr, len, 0) < 0) return -1;
    return sock;
}

bool http_raw_send(int sockfd, const char *data, size_t len) {
    return send(sockfd, data, len, 0) >= 0;
}

void http_raw_response_end(httpd_req_t *req, int sockfd) {
    /* We wrote the response straight to the socket, bypassing
     * httpd_resp_send()/httpd_resp_send_chunk() entirely, so httpd has no
     * idea the response is finished. Tell it explicitly so the session
     * slot is freed immediately — same reasoning as relay_bytes_httpd()
     * in proxy_relay.c. */
    httpd_sess_trigger_close(req->handle, sockfd);
}

bool host_matches_bridge(const char *host_no_port) {
    if (strcmp(host_no_port, GW_IP_STR) == 0) return true;
    char sta_ip[16] = {0};
    wifi_manager_get_ip(sta_ip, sizeof(sta_ip));
    if (sta_ip[0] != '\0' && strcmp(sta_ip, "--") != 0 &&
        strcmp(host_no_port, sta_ip) == 0) return true;
    return false;
}

bool is_foreign_host(httpd_req_t *req) {
    char host[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        return false;
    }
    char host_no_port[128];
    strncpy(host_no_port, host, sizeof(host_no_port) - 1);
    host_no_port[sizeof(host_no_port) - 1] = '\0';
    char *colon = strchr(host_no_port, ':');
    if (colon) *colon = '\0';
    return !host_matches_bridge(host_no_port);
}

void url_encode(const char *src, char *dst, size_t dst_len) {
    size_t w = 0;
    for (const char *p = src; *p && w + 4 < dst_len; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[w++] = c;
        } else {
            w += snprintf(dst + w, dst_len - w, "%%%02X", c);
        }
    }
    dst[w] = 0;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(const char *src, char *dst, size_t dst_len) {
    size_t w = 0;
    for (const char *p = src; *p && w + 1 < dst_len; p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = hex_val(p[1]);
            int lo = hex_val(p[2]);
            if (hi >= 0 && lo >= 0) {
                dst[w++] = (char)((hi << 4) | lo);
                p += 2;
                continue;
            }
        }
        dst[w++] = (*p == '+') ? ' ' : *p;
    }
    dst[w] = 0;
}
