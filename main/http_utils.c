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
