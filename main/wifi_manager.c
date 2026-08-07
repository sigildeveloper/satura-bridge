#include <stdlib.h>

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

#define WIFI_SCAN_TIMEOUT_MS 5000

static volatile bool scan_in_progress = false;
static wifi_scan_result_t scan_results[WIFI_MAX_SCAN_RESULTS];
static int scan_result_count = 0;
static portMUX_TYPE scan_mux = portMUX_INITIALIZER_UNLOCKED;

/* Index into the saved-network list currently being attempted, and the
 * ranked candidate list built from scan + saved networks */
static wifi_network_t candidate_list[WIFI_MAX_SAVED_NETWORKS];
static int candidate_rssi[WIFI_MAX_SAVED_NETWORKS];
static int candidate_count = 0;
static int candidate_index = 0;

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

static void process_scan_results(void);
static void build_candidate_list(void);

/* ============================================================
 * Credentials
 * ============================================================ */

void wifi_manager_set_credentials(const char *ssid, const char *pass) {
    nvs_storage_add_network(ssid, pass);
}

void wifi_manager_clear_credentials(void) {
    nvs_storage_clear_networks();
    esp_wifi_disconnect();
}

bool wifi_manager_has_credentials(void) {
    wifi_network_t tmp[1];
    return nvs_storage_load_networks(tmp, 1) > 0;
}

void wifi_manager_load_saved_credentials(void) {
    /* No-op now — networks are read on-demand from NVS via
     * wifi_manager_has_credentials() / build_candidate_list(). */
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
    r = candidate_index;
    taskEXIT_CRITICAL(&wifi_mux);
    return r;
}

int wifi_manager_get_max_retries(void) {
    int c;
    taskENTER_CRITICAL(&wifi_mux);
    c = candidate_count > 0 ? candidate_count : 1;
    taskEXIT_CRITICAL(&wifi_mux);
    return c;
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

static void try_connect_candidate(int idx) {
    if (idx < 0 || idx >= candidate_count) {
        ESP_LOGW(TAG, "[WIFI] no more candidates, giving up");
        app_state_set(APP_WIFI_FAILED);
        return;
    }

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, candidate_list[idx].ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, candidate_list[idx].pass, sizeof(cfg.sta.password) - 1);
    bool has_pass = (strlen(candidate_list[idx].pass) != 0);
    cfg.sta.threshold.authmode = has_pass ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.scan_method = WIFI_FAST_SCAN;
    cfg.sta.pmf_cfg.capable  = true;
    cfg.sta.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);

    taskENTER_CRITICAL(&wifi_mux);
    strncpy(wifi_ssid, candidate_list[idx].ssid, sizeof(wifi_ssid) - 1);
    strncpy(wifi_pass, candidate_list[idx].pass, sizeof(wifi_pass) - 1);
    taskEXIT_CRITICAL(&wifi_mux);

    ESP_LOGI(TAG, "[WIFI] trying candidate %d/%d: %s (rssi %d)",
             idx + 1, candidate_count, candidate_list[idx].ssid, candidate_rssi[idx]);

    esp_wifi_connect();
}

static void wifi_connect_flow_task(void *arg) {
    (void)arg;

    wifi_scan_config_t scan_cfg = {0};
    esp_wifi_scan_start(&scan_cfg, true);
    process_scan_results();
    scan_in_progress = false;

    build_candidate_list();

    taskENTER_CRITICAL(&wifi_mux);
    wifi_retries        = 0;
    wifi_retry_delay_ms = WIFI_RETRY_BASE_MS;
    taskEXIT_CRITICAL(&wifi_mux);

    if (candidate_count == 0) {
        ESP_LOGW(TAG, "[WIFI] no saved networks visible");
        app_state_set(APP_WIFI_FAILED);
        vTaskDelete(NULL);
        return;
    }

    try_connect_candidate(candidate_index);
    vTaskDelete(NULL);
}

void wifi_manager_start_connect(void) {
    app_state_set(APP_WIFI_CONNECTING);
    safe_task_create(wifi_connect_flow_task, "wifi_conn", 4096, NULL, 5, NULL);
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
            candidate_index++;
            if (candidate_index < candidate_count) {
                /* Try next candidate immediately, no delay needed —
                    * we're just moving to a different already-scanned network */
                try_connect_candidate(candidate_index);
            } else {
                ESP_LOGW(TAG, "[WIFI] exhausted all %d candidate(s)", candidate_count);
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

/* ============================================================
 * Multi-network management
 * ============================================================ */

int wifi_manager_get_saved_networks(wifi_network_t *out, int max_count) {
    return nvs_storage_load_networks(out, max_count);
}

bool wifi_manager_add_network(const char *ssid, const char *pass) {
    return nvs_storage_add_network(ssid, pass);
}

bool wifi_manager_remove_network(int index) {
    return nvs_storage_remove_network(index);
}

/* ============================================================
 * Scanning
 * ============================================================ */

bool wifi_manager_scan_in_progress(void) {
    return scan_in_progress;
}

int wifi_manager_get_scan_results(wifi_scan_result_t *out, int max_count) {
    int n;
    taskENTER_CRITICAL(&scan_mux);
    n = scan_result_count;
    if (n > max_count) n = max_count;
    memcpy(out, scan_results, sizeof(wifi_scan_result_t) * n);
    taskEXIT_CRITICAL(&scan_mux);
    return n;
}

static void process_scan_results(void) {
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        taskENTER_CRITICAL(&scan_mux);
        scan_result_count = 0;
        taskEXIT_CRITICAL(&scan_mux);
        return;
    }
    if (ap_count > 32) ap_count = 32; /* sanity cap for the temp buffer */

    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!records) return;

    uint16_t actual = ap_count;
    if (esp_wifi_scan_get_ap_records(&actual, records) != ESP_OK) {
        free(records);
        return;
    }

    wifi_network_t saved[WIFI_MAX_SAVED_NETWORKS];
    int saved_count = nvs_storage_load_networks(saved, WIFI_MAX_SAVED_NETWORKS);

    taskENTER_CRITICAL(&scan_mux);
    scan_result_count = 0;
    for (int i = 0; i < actual && scan_result_count < WIFI_MAX_SCAN_RESULTS; i++) {
        /* Deduplicate by SSID, keep strongest RSSI */
        bool dup = false;
        for (int j = 0; j < scan_result_count; j++) {
            if (strcmp(scan_results[j].ssid, (char *)records[i].ssid) == 0) {
                dup = true;
                if (records[i].rssi > scan_results[j].rssi)
                    scan_results[j].rssi = records[i].rssi;
                break;
            }
        }
        if (dup) continue;

        strncpy(scan_results[scan_result_count].ssid,
                (char *)records[i].ssid, sizeof(scan_results[0].ssid) - 1);
        scan_results[scan_result_count].ssid[sizeof(scan_results[0].ssid) - 1] = '\0';
        scan_results[scan_result_count].rssi = records[i].rssi;

        bool is_saved = false;
        for (int k = 0; k < saved_count; k++) {
            if (strcmp(saved[k].ssid, (char *)records[i].ssid) == 0) {
                is_saved = true;
                break;
            }
        }
        scan_results[scan_result_count].saved = is_saved;
        scan_result_count++;
    }
    taskEXIT_CRITICAL(&scan_mux);

    free(records);
}

/* Build a ranked candidate list: saved networks that are currently
 * visible, sorted by descending RSSI. Called right before we start
 * trying to connect. */
static void build_candidate_list(void) {
    wifi_network_t saved[WIFI_MAX_SAVED_NETWORKS];
    int saved_count = nvs_storage_load_networks(saved, WIFI_MAX_SAVED_NETWORKS);

    candidate_count = 0;

    taskENTER_CRITICAL(&scan_mux);
    for (int i = 0; i < saved_count; i++) {
        int best_rssi = -127;
        bool visible = false;
        for (int j = 0; j < scan_result_count; j++) {
            if (strcmp(saved[i].ssid, scan_results[j].ssid) == 0) {
                visible = true;
                if (scan_results[j].rssi > best_rssi) best_rssi = scan_results[j].rssi;
            }
        }
        if (visible) {
            candidate_list[candidate_count]  = saved[i];
            candidate_rssi[candidate_count]  = best_rssi;
            candidate_count++;
        }
    }
    taskEXIT_CRITICAL(&scan_mux);

    /* Simple insertion sort by descending RSSI — candidate_count <= 6 */
    for (int i = 1; i < candidate_count; i++) {
        wifi_network_t key_net = candidate_list[i];
        int key_rssi = candidate_rssi[i];
        int j = i - 1;
        while (j >= 0 && candidate_rssi[j] < key_rssi) {
            candidate_list[j + 1] = candidate_list[j];
            candidate_rssi[j + 1] = candidate_rssi[j];
            j--;
        }
        candidate_list[j + 1] = key_net;
        candidate_rssi[j + 1] = key_rssi;
    }

    candidate_index = 0;
}

static void wifi_scan_task(void *arg) {
    (void)arg;
    wifi_scan_config_t scan_cfg = {0};
    esp_wifi_scan_start(&scan_cfg, true); /* blocking scan, runs in this task */
    process_scan_results();
    scan_in_progress = false;
    vTaskDelete(NULL);
}

void wifi_manager_scan_start(void) {
    if (scan_in_progress) return;
    scan_in_progress = true;
    safe_task_create(wifi_scan_task, "wifi_scan", 4096, NULL, 4, NULL);
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
