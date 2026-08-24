#pragma once
#include "esp_http_server.h"

esp_err_t handler_root(httpd_req_t *req);
esp_err_t handler_setup_get(httpd_req_t *req);
esp_err_t handler_setup_post(httpd_req_t *req);
esp_err_t handler_reset(httpd_req_t *req);
esp_err_t handler_proxy_get(httpd_req_t *req);
esp_err_t handler_proxy_post(httpd_req_t *req);
esp_err_t handler_networks_get(httpd_req_t *req);
esp_err_t handler_networks_add_post(httpd_req_t *req);
esp_err_t handler_networks_delete(httpd_req_t *req);
esp_err_t handler_networks_scan(httpd_req_t *req);
esp_err_t handler_networks_connect_get(httpd_req_t *req);
esp_err_t handler_reboot(httpd_req_t *req);
esp_err_t handler_favicon(httpd_req_t *req);
esp_err_t handler_404(httpd_req_t *req, httpd_err_code_t err);
