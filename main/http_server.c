#include "esp_http_server.h"

#include "http_server.h"
#include "http_routes.h"
#include "proxy_relay.h"
#include "clipboard.h"

static httpd_handle_t http_server = NULL;

void http_server_start(void) {
    httpd_config_t cfg    = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size        = 12288;
    cfg.max_open_sockets  = 2;
    cfg.max_uri_handlers  = 20;
    cfg.uri_match_fn      = httpd_uri_match_wildcard;
    if (httpd_start(&http_server, &cfg) == ESP_OK) {
        static const httpd_uri_t uris[] = {
            { "/",                 HTTP_GET,  handler_root,               NULL },
            { "/setup",            HTTP_POST, handler_setup_post,         NULL },
            { "/setup",            HTTP_GET,  handler_setup_get,          NULL },
            { "/reset",            HTTP_GET,  handler_reset,              NULL },
            { "/reboot",           HTTP_GET,  handler_reboot,             NULL },
            { "/favicon.ico",      HTTP_GET,  handler_favicon,            NULL },
            { "/networks",         HTTP_GET,  handler_networks_get,       NULL },
            { "/networks/add",     HTTP_POST, handler_networks_add_post,  NULL },
            { "/networks/delete",  HTTP_GET,  handler_networks_delete,    NULL },
            { "/networks/scan",    HTTP_GET,  handler_networks_scan,      NULL },
            { "/networks/connect", HTTP_GET,  handler_networks_connect_get, NULL },
            { "/proxy",            HTTP_GET,  handler_proxy_get,          NULL },
            { "/proxy",            HTTP_POST, handler_proxy_post,         NULL },
            { "/name",             HTTP_GET,  handler_name_get,           NULL },
            { "/name",             HTTP_POST, handler_name_post,          NULL },
            { "/clip",             HTTP_GET,  handler_clip_get,           NULL },
            { "/clip/add",         HTTP_POST, handler_clip_add_post,      NULL },
            { "/clip/action",      HTTP_GET,  handler_clip_action_get,    NULL },
            { "/*",                HTTP_GET,  handler_proxy_relay,        NULL },
            { "/*",                HTTP_POST, handler_proxy_relay,        NULL },
        };
        for (int i = 0; i < 19; i++)
            httpd_register_uri_handler(http_server, &uris[i]);
        httpd_register_err_handler(http_server,
                                   HTTPD_404_NOT_FOUND, handler_404);
    }
}
