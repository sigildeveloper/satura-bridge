#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "lwip/sockets.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "http_server.h"
#include "config.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "bt_pan.h"
#include "http_utils.h"
#include "task_utils.h"
#include "uptime.h"
#include "nvs_storage.h"

#define PROJECT_VERSION "v0.0.10"
#define TELEGRAM_CHAT   "https://t.me/nnmidletschat"
#define PAGE_FOOTER \
    "<hr><p>" \
    "Community: <a href='" TELEGRAM_CHAT "'>t.me/nnmidletschat</a>" \
    "<br>Author: @sigdev" \
    " | " PROJECT_VERSION \
    "</p>"

static httpd_handle_t http_server = NULL;

static bool captive_check(httpd_req_t *req) {
    app_state_t st = app_state_get();
    if (st == APP_BRIDGE) return true;
    char host[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK
        && strstr(host, GW_IP_STR) == NULL) {
        char host_no_port[64];
        strncpy(host_no_port, host, sizeof(host_no_port) - 1);
        host_no_port[sizeof(host_no_port)-1] = '\0';
        char *colon = strchr(host_no_port, ':');
        if (colon) *colon = '\0';
        struct in_addr tmp;
        if (inet_pton(AF_INET, host_no_port, &tmp) == 0) {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "http://" GW_IP_STR "/");
            httpd_resp_send(req, NULL, 0);
            return false;
        }
    }
    return true;
}

/* ---- Static page templates ---- */

static const char PAGE_SETUP[] =
    "<html><head><title>Satura Bridge Setup</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Satura Bridge Setup</h2><hr>"
    "<p><a href='/networks'>Scan for networks</a></p>"
    "<p>...or enter manually:</p>"
    "<form action='/setup' method='post'>"
    "<p>SSID:<br>"
    "<input type='text' name='ssid' size='20' maxlength='32'></p>"
    "<p>Password: (optional)<br>"
    "<input type='password' name='pass' size='20' maxlength='63'></p>"
    "<p><input type='submit' value='Connect' style='font-size:110%;'></p>"
    "</form><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reboot' style='color:#e74c3c;'>Reboot</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_STATUS_FMT[] =
    "<html><head><title>Satura Bridge Status</title>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='30'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Satura Bridge</h2><hr>"
    "<div style='text-align:left;background:#ecf0f1;padding:15px;'>"
    "<b>WiFi:</b> %s<br>"
    "<b>IP:</b> %s<br>"
    "<b>WiFi RSSI:</b> %d dBm<br>"
    "<b>BT RSSI:</b> %d dBm<br>"
    "<b>Uptime:</b> %" PRIu32 "d %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 "<br>"
    "<b>Free heap:</b> %d KB"
    "</div><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reset'>Forget WiFi</a><br>"
    "<a href='/networks'>Manage Networks</a><br>"
    "<a href='/reboot' style='color:#e74c3c;'>Reboot</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_NO_WIFI_FMT[] =
    "<html><head><title>WiFi Lost</title>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='5'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>WiFi Lost</h2><hr>"
    "<p>Reconnecting...</p>"
    "<p>Attempt %d</p>"
    "<small>This page refreshes automatically.</small><hr>"
    "<a href='/'>Reload</a><br>"
    "<a href='/reset'>Forget WiFi</a>"
    "<br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_SETUP_FAILED[] =
    "<html><head><title>Connection Failed</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Connection Failed</h2><hr>"
    "<p style='color:#e74c3c;'>Could not connect.<br>Check SSID and password.</p>"
    "<p><a href='/networks'>Manage networks</a></p>"
    "<form action='/setup' method='post'>";

static const char PAGE_NETWORKS_HEADER[] =
    "<html><head><title>Wi-Fi Networks</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Wi-Fi Networks</h2><hr>";

static const char PAGE_NETWORKS_FOOTER[] =
    "<a href='/'>Back</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static const char PAGE_CONNECT_PROMPT_FMT[] =
    "<html><head><title>Connect</title>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='format-detection' content='telephone=no'>"
    "</head>"
    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
    "<h2>Connect to network</h2><hr>"
    "<p><b>%s</b></p>"
    "<form action='/networks/add' method='post'>"
    "<input type='hidden' name='ssid' value=\"%s\">"
    "<p>Password: (leave empty if open)<br>"
    "<input type='password' name='pass' size='20' maxlength='63' autofocus></p>"
    "<p><input type='submit' value='Add &amp; Connect' style='font-size:110%%;'></p>"
    "</form><hr>"
    "<a href='/networks'>Cancel</a>"
    "<br><br><small>" PAGE_FOOTER "</small>"
    "</body></html>";

static esp_err_t handler_root(httpd_req_t *req) {
    if (!captive_check(req)) return ESP_OK;
    set_no_cache(req, "text/html");

    if (get_bt_connected()) {
        bt_pan_request_rssi_poll();
    }

    app_state_t st    = app_state_get();
    bool w_conn       = get_wifi_connected();
    int8_t rw         = get_wifi_rssi();
    int8_t rb         = get_bt_rssi();

    int retries = wifi_manager_get_retries();

    bool show_status = (st == APP_BRIDGE) ||
                       (st == APP_WAIT_BT && w_conn);

    if (!show_status) {
        char esc[64] = {0};
        switch (st) {
            case APP_WAIT_BT:
            case APP_NO_WIFI:
                return httpd_resp_sendstr(req, PAGE_SETUP);
            case APP_WIFI_FAILED:
                return httpd_resp_sendstr(req, PAGE_SETUP_FAILED);
            case APP_WIFI_CONNECTING: {
                char ssid_buf[64];
                wifi_manager_get_ssid(ssid_buf, sizeof(ssid_buf));
                html_escape(ssid_buf, esc, sizeof(esc));
                char *page = malloc(2048);
                if (!page) return ESP_ERR_NO_MEM;
                snprintf(page, 2048,
                    "<html><head><title>Connecting...</title>"
                    "<meta charset='utf-8'>"
                    "<meta http-equiv='refresh' content='5'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<meta name='format-detection' content='telephone=no'>"
                    "</head>"
                    "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
                    "<h2>Connecting</h2><hr>"
                    "<p>Network:</p><div style='word-wrap:break-word;'><b>%s</b></div>"
                    "<p>Trying network %d of %d</p>"
                    "<small>This page refreshes automatically.</small><hr>"
                    "<a href='/'>Reload</a><br>"
                    "<a href='/reset'>Forget WiFi</a>"
                    "<br><small>" PAGE_FOOTER "</small>"
                    "</body></html>",
                    esc, retries + 1, wifi_manager_get_max_retries());
                esp_err_t r = httpd_resp_sendstr(req, page);
                free(page);
                return r;
            }
            case APP_BRIDGE_NO_WIFI: {
                char *page = malloc(1024);
                if (!page) return ESP_ERR_NO_MEM;
                snprintf(page, 1024, PAGE_NO_WIFI_FMT, retries + 1);
                esp_err_t r = httpd_resp_sendstr(req, page);
                free(page);
                return r;
            }
            default: break;
        }
    }

    char esc[64] = {0};
    char ssid_buf2[64];
    wifi_manager_get_ssid(ssid_buf2, sizeof(ssid_buf2));
    html_escape(ssid_buf2, esc, sizeof(esc));
    uint32_t up = uptime_seconds();
    char *page = malloc(3072);
    if (!page) return ESP_ERR_NO_MEM;
    char ip_buf[16];
    wifi_manager_get_ip(ip_buf, sizeof(ip_buf));
    snprintf(page, 3072, PAGE_STATUS_FMT,
             esc, ip_buf, (int)rw, (int)rb,
             up / 86400, (up % 86400) / 3600, (up % 3600) / 60, up % 60,
             (int)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static esp_err_t handler_setup_get(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_setup_post(httpd_req_t *req) {
    char buf[256] = {0};
    int rec = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (rec <= 0) return ESP_FAIL;
    buf[rec] = '\0';
    char ns[64] = {0}, np[64] = {0};
    if (httpd_query_key_value(buf, "ssid", ns, sizeof(ns)) == ESP_OK) {
        httpd_query_key_value(buf, "pass", np, sizeof(np));

        wifi_manager_set_credentials(ns, np);
        app_state_set(APP_WIFI_CONNECTING);
        wifi_manager_start_connect();

        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        return httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t handler_reset(httpd_req_t *req) {
    wifi_manager_clear_credentials();
    app_state_set(APP_NO_WIFI);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_get(httpd_req_t *req) {
    set_no_cache(req, "text/html");

    char *page = malloc(4096);
    if (!page) return ESP_ERR_NO_MEM;
    size_t len = 0;

    len += snprintf(page + len, 4096 - len, "%s", PAGE_NETWORKS_HEADER);

    /* Saved networks */
    wifi_network_t saved[WIFI_MAX_SAVED_NETWORKS];
    int saved_count = wifi_manager_get_saved_networks(saved, WIFI_MAX_SAVED_NETWORKS);

    len += snprintf(page + len, 4096 - len,
        "<div style='text-align:left;background:#ecf0f1;padding:10px;'>"
        "<b>Saved (%d/%d):</b><br>", saved_count, WIFI_MAX_SAVED_NETWORKS);

    if (saved_count == 0) {
        len += snprintf(page + len, 4096 - len, "<i>None yet</i><br>");
    }
    for (int i = 0; i < saved_count; i++) {
        char esc[64] = {0};
        html_escape(saved[i].ssid, esc, sizeof(esc));
        len += snprintf(page + len, 4096 - len,
            "%s &nbsp; <a href='/networks/delete?idx=%d' style='color:#e74c3c;'>[remove]</a><br>",
            esc, i);
    }
    len += snprintf(page + len, 4096 - len, "</div><br>");

    /* Add network form */
    len += snprintf(page + len, 4096 - len,
        "<form action='/networks/add' method='post'>"
        "<p>SSID:<br><input type='text' name='ssid' size='20' maxlength='32'></p>"
        "<p>Password: (optional)<br><input type='password' name='pass' size='20' maxlength='63'></p>"
        "<p><input type='submit' value='Add Network' style='font-size:110%%;'></p>"
        "</form><hr>");

    /* Scan results */
    bool scanning = wifi_manager_scan_in_progress();
    if (scanning) {
        len += snprintf(page + len, 4096 - len,
            "<p>Scanning...</p>"
            "<meta http-equiv='refresh' content='2'>");
    } else {
        wifi_scan_result_t results[WIFI_MAX_SCAN_RESULTS];
        int n = wifi_manager_get_scan_results(results, WIFI_MAX_SCAN_RESULTS);

        len += snprintf(page + len, 4096 - len,
            "<div style='text-align:left;background:#ecf0f1;padding:10px;'>"
            "<b>Found nearby:</b><br>");
        if (n == 0) {
            len += snprintf(page + len, 4096 - len, "<i>No scan results yet</i><br>");
        }
        for (int i = 0; i < n && len < 3600; i++) {
            char esc[64] = {0};
            html_escape(results[i].ssid, esc, sizeof(esc));
            char enc[100] = {0};
            url_encode(results[i].ssid, enc, sizeof(enc));
            len += snprintf(page + len, 4096 - len,
                "<a href='/networks/connect?ssid=%s'>%s (%d dBm)</a>%s<br>",
                enc, esc, (int)results[i].rssi,
                results[i].saved ? " &nbsp;<i>saved</i>" : "");
        }
        len += snprintf(page + len, 4096 - len, "</div><br>");
        len += snprintf(page + len, 4096 - len,
            "<a href='/networks/scan'>Scan again</a><br>");
    }

    len += snprintf(page + len, 4096 - len, "%s", PAGE_NETWORKS_FOOTER);

    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static esp_err_t handler_networks_add_post(httpd_req_t *req) {
    char buf[256] = {0};
    int rec = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (rec <= 0) return ESP_FAIL;
    buf[rec] = '\0';
    char ns[64] = {0}, np[64] = {0};
    if (httpd_query_key_value(buf, "ssid", ns, sizeof(ns)) == ESP_OK) {
        httpd_query_key_value(buf, "pass", np, sizeof(np));
        wifi_manager_add_network(ns, np);
        app_state_set(APP_WIFI_CONNECTING);
        wifi_manager_start_connect();
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_delete(httpd_req_t *req) {
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char idx_s[8] = {0};
        if (httpd_query_key_value(query, "idx", idx_s, sizeof(idx_s)) == ESP_OK) {
            int idx = atoi(idx_s);
            wifi_manager_remove_network(idx);
        }
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/networks");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_scan(httpd_req_t *req) {
    wifi_manager_scan_start();
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/networks");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_networks_connect_get(httpd_req_t *req) {
    set_no_cache(req, "text/html");

    char query[128] = {0};
    char ssid_raw[100] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char ssid_enc[100] = {0};
        if (httpd_query_key_value(query, "ssid", ssid_enc, sizeof(ssid_enc)) == ESP_OK) {
            url_decode(ssid_enc, ssid_raw, sizeof(ssid_raw));
        }
    }

    if (strlen(ssid_raw) == 0) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/networks");
        return httpd_resp_send(req, NULL, 0);
    }

    char esc[64] = {0};
    html_escape(ssid_raw, esc, sizeof(esc));

    char *page = malloc(1536);
    if (!page) return ESP_ERR_NO_MEM;
    snprintf(page, 1536, PAGE_CONNECT_PROMPT_FMT, esc, esc);
    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

static void reboot_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static esp_err_t handler_reboot(httpd_req_t *req) {
    httpd_resp_sendstr(req,
        "<html><body><p>Rebooting...</p></body></html>");
    safe_task_create(reboot_task, "reboot", 2048, NULL, 3, NULL);
    return ESP_OK;
}

static esp_err_t handler_favicon(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handler_404(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" GW_IP_STR "/");
    return httpd_resp_send(req, NULL, 0);
}

void http_server_start(void) {
    httpd_config_t cfg    = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size        = 8192;
    cfg.max_open_sockets  = 2;
    cfg.max_uri_handlers  = 12;
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
        };
        for (int i = 0; i < 11; i++)
            httpd_register_uri_handler(http_server, &uris[i]);
        httpd_register_err_handler(http_server,
                                   HTTPD_404_NOT_FOUND, handler_404);
    }
}
