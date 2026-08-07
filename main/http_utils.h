#pragma once
#include <stddef.h>
#include "esp_http_server.h"

void html_escape(const char *src, char *dst, size_t n);
void set_no_cache(httpd_req_t *req, const char *type);


void url_encode(const char *src, char *dst, size_t dst_len);
void url_decode(const char *src, char *dst, size_t dst_len);
