#include <string.h>
#include <strings.h>
#include "lwip/sockets.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "proxy_relay.h"
#include "config.h"
#include "http_utils.h"
#include "proxy_gateway.h"

static const char *TAG = "proxy_relay";

#define HEAD_BUF_SIZE 2048

static bool is_hop_by_hop_header(const char *name) {
    return strcasecmp(name, "Content-Length") == 0 ||
           strcasecmp(name, "Transfer-Encoding") == 0 ||
           strcasecmp(name, "Connection") == 0;
}

static void relay_bytes_httpd(httpd_req_t *req, int upstream_sock) {
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(upstream_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Read the upstream's status line + headers byte-by-byte until the
     * blank line that ends them. Heap-allocated — 2KB is too much to
     * safely add to an already-deep call stack inside the httpd task. */
    char *head = malloc(HEAD_BUF_SIZE);
    if (!head) return;
    head[0] = '\0';
    int head_len = 0;
    bool found_terminator = false;

    while (head_len < HEAD_BUF_SIZE - 1 && !found_terminator) {
        int n = recv(upstream_sock, head + head_len, HEAD_BUF_SIZE - 1 - head_len, 0);
        if (n <= 0) break;
        head_len += n;
        head[head_len] = '\0';

        /* Scan from a bit before this chunk started, in case the
            * \r\n\r\n terminator straddles two recv() calls. */
        int scan_from = head_len - n - 3;
        if (scan_from < 0) scan_from = 0;
        for (int i = scan_from; i + 4 <= head_len; i++) {
            if (head[i] == '\r' && head[i + 1] == '\n' &&
                head[i + 2] == '\r' && head[i + 3] == '\n') {
                found_terminator = true;
                break;
            }
        }
    }

    /* Parse "HTTP/1.1 200 OK" -> "200 OK" for httpd_resp_set_status */
    char status[32] = "200 OK";
    char *sp1 = strchr(head, ' ');
    if (sp1) {
        char *line_end = strstr(sp1, "\r\n");
        if (line_end) {
            size_t len = line_end - (sp1 + 1);
            if (len < sizeof(status)) {
                memcpy(status, sp1 + 1, len);
                status[len] = '\0';
            }
        }
    }
    httpd_resp_set_status(req, status);

    /* Forward every response header except hop-by-hop ones that
     * esp_http_server manages itself (we're using chunked transfer,
     * so Content-Length/Transfer-Encoding from upstream would conflict). */
    char *line_start = strstr(head, "\r\n");
    if (line_start) line_start += 2; /* skip past the status line */
    while (line_start && *line_start && strncmp(line_start, "\r\n", 2) != 0) {
        char *line_end = strstr(line_start, "\r\n");
        if (!line_end) break;

        char *colon = memchr(line_start, ':', line_end - line_start);
        if (colon) {
            char name[64] = {0};
            size_t name_len = colon - line_start;
            if (name_len < sizeof(name)) {
                memcpy(name, line_start, name_len);
                name[name_len] = '\0';

                if (!is_hop_by_hop_header(name)) {
                    char *val_start = colon + 1;
                    while (*val_start == ' ') val_start++;
                    size_t val_len = line_end - val_start;

                    /* httpd_resp_set_hdr() only stores the pointer, not a
                        * copy — the value must stay valid until the response
                        * is sent, so allocate it instead of using a stack
                        * buffer that goes out of scope. */
                    char *value = malloc(val_len + 1);
                    if (value) {
                        memcpy(value, val_start, val_len);
                        value[val_len] = '\0';
                        httpd_resp_set_hdr(req, name, value);
                        /* Intentionally not freed here — esp_http_server
                            * needs it until the response finishes sending,
                            * which happens right after this function returns.
                            * The whole request's memory is reclaimed when the
                            * connection's task exits. */
                    }
                }
            }
        }
        line_start = line_end + 2;
    }

    /* If the read above grabbed some body bytes along with the headers
    * (a single recv() can return more than just the header block),
    * forward that leftover chunk first before reading more. */
    int body_start = -1;
    for (int i = 0; i + 4 <= head_len; i++) {
        if (head[i] == '\r' && head[i + 1] == '\n' &&
            head[i + 2] == '\r' && head[i + 3] == '\n') {
            body_start = i + 4;
            break;
        }
    }
    if (body_start >= 0 && body_start < head_len) {
        httpd_resp_send_chunk(req, head + body_start, head_len - body_start);
    }
    free(head);

    /* Now relay the rest of the body. */
    char buf[512];
    while (1) {
        int n = recv(upstream_sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) break;
    }
    httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t handler_proxy_relay(httpd_req_t *req) {
    /* This handler is reached via the wildcard "*" route for ANY path
     * that isn't an exact match elsewhere — esp_http_server dispatches
     * purely on the URI path, before any of our own code runs, so the
     * proxy-enabled/foreign-host checks in handler_root() never apply
     * here. Re-check the same conditions ourselves; if this isn't
     * actually a proxy-eligible request, behave like the captive
     * portal's 404 handler instead of attempting (and failing) a relay. */
    if (!proxy_gateway_is_enabled() || !is_foreign_host(req)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://" GW_IP_STR "/");
        return httpd_resp_send(req, NULL, 0);
    }

    char host[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    char *colon = strchr(host, ':');
    if (colon) *colon = '\0';

    char upstream_host[64] = {0};
    uint16_t upstream_port = 0;
    proxy_gateway_get_upstream(upstream_host, sizeof(upstream_host), &upstream_port);
    if (upstream_host[0] == '\0' || upstream_port == 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int upstream_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (upstream_sock < 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    /* Apply the same 5s timeout to the connect + request-send phase as
     * relay_bytes_httpd() applies to the response phase — otherwise a
     * connect() that hangs (firewalled/black-holed upstream) or an
     * upstream that accepts but never reads ties up this worker
     * indefinitely. With max_open_sockets=2, one stuck request halves
     * the bridge's proxy capacity. */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(upstream_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(upstream_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in upstream_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(upstream_port),
    };
    if (inet_pton(AF_INET, upstream_host, &upstream_addr.sin_addr) != 1 ||
        connect(upstream_sock, (struct sockaddr *)&upstream_addr, sizeof(upstream_addr)) != 0) {
        ESP_LOGW(TAG, "failed to connect to upstream %s:%d", upstream_host, upstream_port);
        close(upstream_sock);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    /* Rebuild the request line in absolute-URI form: GET http://host/uri HTTP/1.1 */
    const char *method = (req->method == HTTP_POST) ? "POST" : "GET";
    char req_line[1200];
    snprintf(req_line, sizeof(req_line), "%s http://%s%s HTTP/1.1\r\n",
             method, host, req->uri);
    send(upstream_sock, req_line, strlen(req_line), 0);

    char host_hdr[160];
    snprintf(host_hdr, sizeof(host_hdr), "Host: %s\r\n", host);
    send(upstream_sock, host_hdr, strlen(host_hdr), 0);

    /* Forward the phone's real headers — many WAP gateways use these to
     * distinguish legitimate mobile browsers, and Cookie/Content-Type
     * are needed for logins and form submissions to work at all. */
    static const char *forward_headers[] = {
        "User-Agent", "Accept", "Accept-Language", "Accept-Charset",
        "Cookie", "Content-Type", NULL
    };
    for (int i = 0; forward_headers[i]; i++) {
        char val[512] = {0};
        if (httpd_req_get_hdr_value_str(req, forward_headers[i], val, sizeof(val)) == ESP_OK) {
            char hdr_line[560];
            snprintf(hdr_line, sizeof(hdr_line), "%s: %s\r\n", forward_headers[i], val);
            send(upstream_sock, hdr_line, strlen(hdr_line), 0);
        }
    }

    /* Forward the request body for POST (form submissions, logins). */
    size_t remaining = req->content_len;
    if (remaining > 0) {
        char len_hdr[64];
        snprintf(len_hdr, sizeof(len_hdr), "Content-Length: %d\r\n", (int)remaining);
        send(upstream_sock, len_hdr, strlen(len_hdr), 0);
    }

    send(upstream_sock, "Connection: close\r\n\r\n", 22, 0);

    char body_buf[512];
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(body_buf) ? remaining : sizeof(body_buf);
        int n = httpd_req_recv(req, body_buf, chunk);
        if (n <= 0) break;
        send(upstream_sock, body_buf, n, 0);
        remaining -= n;
    }

    ESP_LOGI(TAG, "relaying: %s", req_line);

    relay_bytes_httpd(req, upstream_sock);

    shutdown(upstream_sock, SHUT_RDWR);
    close(upstream_sock);
    return ESP_OK;
}
