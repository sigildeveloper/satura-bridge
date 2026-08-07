#include <string.h>
#include "http_utils.h"

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
