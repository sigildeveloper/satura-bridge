#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "wifi_manager.h"
#include "app_state.h"
#include "nvs_storage.h"
#include "task_utils.h"

static const char *TAG = "wifi_mgr";

#define WIFI_MAX_RETRIES     7
#define WIFI_RETRY_BASE_MS   1000
#define WIFI_RETRY_MAX_MS    30000

static portMUX_TYPE wifi_mux = portMUX_INITIALIZER_UNLOCKED;

static char wifi_ssid[64] = {0};
static char wifi_pass[64] = {0};
static char wifi_ip[16]   = "--";

static volatile bool wifi_ignore_disconnect  = false;
static volatile int  wifi_retries            = 0;
static volatile uint32_t wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;

static wifi_manager_state_cb_t state_change_cb = NULL;

static volatile bool wifi_retry_running    = false;
static volatile bool wifi_recovery_running = false;

static esp_netif_t *sta_netif = NULL;

static esp_event_handler_instance_t wifi_evt_inst = NULL;
static esp_event_handler_instance_t ip_evt_inst   = NULL;

static void wifi_retry_task(void *arg);
static void wifi_recovery_task(void *arg);
static void wifi_soft_reset(void);
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data);

/* ============================================================
 * Credentials
 * ============================================================ */

void wifi_manager_set_credentials(const char *ssid, const char *pass) {
    taskENTER_CRITICAL(&wifi_mux);
    strncpy(wifi_ssid, ssid, sizeof(wifi_ssid) - 1);
    strncpy(wifi_pass, pass, sizeof(wifi_pass) - 1);
    taskEXIT_CRITICAL(&wifi_mux);
    nvs_storage_save(wifi_ssid, wifi_pass);
}

void wifi_manager_clear_credentials(void) {
    taskENTER_CRITICAL(&wifi_mux);
    memset(wifi_ssid, 0, sizeof(wifi_ssid));
    memset(wifi_pass, 0, sizeof(wifi_pass));
    taskEXIT_CRITICAL(&wifi_mux);
    nvs_storage_clear();
    esp_wifi_disconnect();
}

bool wifi_manager_has_credentials(void) {
    taskENTER_CRITICAL(&wifi_mux);
    bool has = (strlen(wifi_ssid) != 0);
    taskEXIT_CRITICAL(&wifi_mux);
    return has;
}

void wifi_manager_load_saved_credentials(void) {
    nvs_storage_load(wifi_ssid, sizeof(wifi_ssid), wifi_pass, sizeof(wifi_pass));
}

void wifi_manager_get_ssid(char *out, size_t len) {
    taskENTER_CRITICAL(&wifi_mux);
    strncpy(out, wifi_ssid, len - 1);
    out[len - 1] = '\0';
    taskEXIT_CRITICAL(&wifi_mux);
}

void wifi_manager_get_ip(char *out, size_t len) {
    taskENTER_CRITICAL(&wifi_mux);
    strncpy(out, wifi_ip, len - 1);
    out[len - 1] = '\0';
    taskEXIT_CRITICAL(&wifi_mux);
}

int wifi_manager_get_retries(void) {
    int r;
    taskENTER_CRITICAL(&wifi_mux);
    r = wifi_retries;
    taskEXIT_CRITICAL(&wifi_mux);
    return r;
}

int wifi_manager_get_max_retries(void) {
    return WIFI_MAX_RETRIES;
}

esp_netif_t *wifi_manager_get_sta_netif(void) {
    return sta_netif;
}

void wifi_manager_set_state_change_cb(wifi_manager_state_cb_t cb) {
    state_change_cb = cb;
}

/* ============================================================
 * Connect / retry
 * ============================================================ */

void wifi_manager_start_connect(void) {
    wifi_config_t cfg = {0};

    taskENTER_CRITICAL(&wifi_mux);
    strncpy((char *)cfg.sta.ssid,     wifi_ssid, sizeof(cfg.sta.ssid)     - 1);
    strncpy((char *)cfg.sta.password, wifi_pass, sizeof(cfg.sta.password) - 1);
    bool has_pass = (strlen(wifi_pass) != 0);
    taskEXIT_CRITICAL(&wifi_mux);

    cfg.sta.threshold.authmode = has_pass ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.scan_method = WIFI_FAST_SCAN;
    cfg.sta.pmf_cfg.capable  = true;
    cfg.sta.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);

    app_state_set(APP_WIFI_CONNECTING);

    taskENTER_CRITICAL(&wifi_mux);
    wifi_retries        = 0;
    wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
    taskEXIT_CRITICAL(&wifi_mux);

    esp_wifi_connect();
}

static void wifi_schedule_retry(uint32_t delay_ms) {
    bool already;
    taskENTER_CRITICAL(&wifi_mux);
    already = wifi_retry_running;
    if (!already) wifi_retry_running = true;
    taskEXIT_CRITICAL(&wifi_mux);
    if (already) return;

    if (xTaskCreate(wifi_retry_task, "wr", 3072,
                    (void *)(uintptr_t)delay_ms, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "[WIFI] failed to create retry task");
        taskENTER_CRITICAL(&wifi_mux);
        wifi_retry_running = false;
        taskEXIT_CRITICAL(&wifi_mux);
    }
}

static void wifi_retry_task(void *arg) {
    uint32_t delay = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(delay));

    if (wifi_manager_has_credentials()) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_wifi_connect();
    }

    taskENTER_CRITICAL(&wifi_mux);
    wifi_retry_running = false;
    taskEXIT_CRITICAL(&wifi_mux);
    vTaskDelete(NULL);
}

/* ============================================================
 * Event handler
 * ============================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {

        taskENTER_CRITICAL(&wifi_mux);
        bool ignore = wifi_ignore_disconnect;
        if (ignore) wifi_ignore_disconnect = false;
        taskEXIT_CRITICAL(&wifi_mux);

        if (ignore) {
            set_wifi_connected(false);
            if (state_change_cb) state_change_cb();
            return;
        }

        set_wifi_connected(false);
        if (state_change_cb) state_change_cb();
        app_state_t st = app_state_get();

        taskENTER_CRITICAL(&wifi_mux);
        strncpy(wifi_ip, "--", sizeof(wifi_ip));
        taskEXIT_CRITICAL(&wifi_mux);

        if (st == APP_WIFI_CONNECTING) {
            taskENTER_CRITICAL(&wifi_mux);
            int retries  = ++wifi_retries;
            uint32_t delay = wifi_retry_delay_ms;
            wifi_retry_delay_ms = (delay * 2 > WIFI_RETRY_MAX_MS)
                                  ? WIFI_RETRY_MAX_MS : delay * 2;
            taskEXIT_CRITICAL(&wifi_mux);

            if (retries < WIFI_MAX_RETRIES) {
                wifi_schedule_retry(delay);
            } else {
                app_state_set(APP_WIFI_FAILED);
            }

        } else if (st == APP_BRIDGE || st == APP_BRIDGE_NO_WIFI) {
            bool has_ssid = wifi_manager_has_credentials();

            if (!has_ssid) {
                app_state_set(APP_NO_WIFI);
                return;
            }
            if (st == APP_BRIDGE) {
                app_state_set(APP_BRIDGE_NO_WIFI);
                taskENTER_CRITICAL(&wifi_mux);
                wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
                wifi_retries = 0;
                taskEXIT_CRITICAL(&wifi_mux);
            }

            taskENTER_CRITICAL(&wifi_mux);
            wifi_retries++;
            uint32_t delay = wifi_retry_delay_ms;
            wifi_retry_delay_ms = (delay * 2 > WIFI_RETRY_MAX_MS)
                                  ? WIFI_RETRY_MAX_MS : delay * 2;
            taskEXIT_CRITICAL(&wifi_mux);

            wifi_schedule_retry(delay);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;

        taskENTER_CRITICAL(&wifi_mux);
        snprintf(wifi_ip, sizeof(wifi_ip), IPSTR, IP2STR(&e->ip_info.ip));
        taskEXIT_CRITICAL(&wifi_mux);

        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            set_wifi_rssi(ap.rssi);
        }

        set_wifi_connected(true);
        if (state_change_cb) state_change_cb();
        bool bt = get_bt_connected();
        app_state_t st = app_state_get();

        taskENTER_CRITICAL(&wifi_mux);
        wifi_retries        = 0;
        wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
        taskEXIT_CRITICAL(&wifi_mux);

        if (bt) {
            app_state_set(APP_BRIDGE);
        } else if (st != APP_WAIT_BT) {
            app_state_set(APP_WAIT_BT);
        }
    }
}

/* ============================================================
 * Init / soft reset / recovery
 * ============================================================ */

static void wifi_register_handlers(void) {
    if (wifi_evt_inst == NULL)
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID,
            wifi_event_handler, NULL, &wifi_evt_inst));
    if (ip_evt_inst == NULL)
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP,
            wifi_event_handler, NULL, &ip_evt_inst));
}

static void wifi_unregister_handlers(void) {
    if (wifi_evt_inst) {
        esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt_inst);
        wifi_evt_inst = NULL;
    }
    if (ip_evt_inst) {
        esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, ip_evt_inst);
        ip_evt_inst = NULL;
    }
}

void wifi_manager_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_register_handlers();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_soft_reset(void) {
    ESP_LOGW(TAG, "[WIFI] soft-reset starting...");

    wifi_config_t saved_cfg = {0};
    esp_wifi_get_config(WIFI_IF_STA, &saved_cfg);

    wifi_unregister_handlers();

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_deinit();
    vTaskDelay(pdMS_TO_TICKS(300));

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    wifi_register_handlers();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &saved_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    ESP_LOGI(TAG, "[WIFI] soft-reset done");
}

static void wifi_recovery_task(void *arg) {
    (void)arg;
    wifi_soft_reset();
    taskENTER_CRITICAL(&wifi_mux);
    wifi_recovery_running = false;
    taskEXIT_CRITICAL(&wifi_mux);
    vTaskDelete(NULL);
}

void wifi_manager_schedule_recovery(void) {
    bool already;
    taskENTER_CRITICAL(&wifi_mux);
    already = wifi_recovery_running;
    if (!already) wifi_recovery_running = true;
    taskEXIT_CRITICAL(&wifi_mux);
    if (!already) {
        safe_task_create(wifi_recovery_task, "wifi_rec", 3072, NULL, 5, NULL);
    }
}
