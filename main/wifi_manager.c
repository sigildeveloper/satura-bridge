#include <stdlib.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"

#include "wifi_manager.h"
#include "app_state.h"
#include "nvs_storage.h"
#include "task_utils.h"

static const char *TAG = "wifi_mgr";

#define WIFI_RETRY_BASE_MS       1000
#define WIFI_RETRY_MAX_MS        30000
#define WIFI_FULL_RETRY_DELAY_MS 15000
#define CAND_MAX_ATTEMPTS        2

/* ============================================================
 * FSM types
 * ============================================================ */

typedef enum {
    WFSM_IDLE,
    WFSM_CONNECTING,
    WFSM_CONNECTED,
    WFSM_RETRY_WAIT,
    WFSM_RECOVERING,
} wifi_fsm_state_t;

typedef enum {
    WEVT_CONNECT_REQUEST,
    WEVT_SCAN_REQUEST,
    WEVT_FORGET_REQUEST,
    WEVT_RECOVERY_REQUEST,
    WEVT_DISCONNECTED,
    WEVT_GOT_IP,
} wifi_evt_type_t;

typedef struct {
    wifi_evt_type_t type;
    esp_ip4_addr_t  ip; /* only valid for WEVT_GOT_IP */
} wifi_evt_t;

typedef enum {
    RETRY_REASON_NONE,
    RETRY_REASON_INITIAL_FAILED,   /* never had an IP this cycle */
    RETRY_REASON_LOST_CONNECTION,  /* was bridging, connection dropped */
} retry_reason_t;

/* ============================================================
 * State — all owned exclusively by wifi_worker_task. Only the
 * mux-protected copies below (wifi_ssid/wifi_ip/etc.) are read by
 * other tasks, via the getter functions.
 * ============================================================ */

static QueueHandle_t wifi_evt_queue = NULL;
static wifi_fsm_state_t fsm_state = WFSM_IDLE;
static retry_reason_t   retry_reason = RETRY_REASON_NONE;
static uint32_t         retry_delay_ms = WIFI_RETRY_BASE_MS;

static wifi_network_t candidate_list[WIFI_MAX_SAVED_NETWORKS];
static int candidate_rssi[WIFI_MAX_SAVED_NETWORKS];
static int candidate_count   = 0;
static int candidate_index   = 0;
static int candidate_attempt = 0;

static wifi_scan_result_t scan_results[WIFI_MAX_SCAN_RESULTS];
static int scan_result_count = 0;

static esp_netif_t *sta_netif = NULL;

static esp_event_handler_instance_t wifi_evt_inst = NULL;
static esp_event_handler_instance_t ip_evt_inst   = NULL;

/* Mux-protected fields shared with other tasks via getters */
static portMUX_TYPE wifi_mux = portMUX_INITIALIZER_UNLOCKED;
static char wifi_ssid[64] = {0};
static char wifi_pass[64] = {0};
static char wifi_ip[16]   = "--";

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data);
static void wifi_soft_reset(void);

/* ============================================================
 * Public: credentials / multi-network storage
 * ============================================================ */

void wifi_manager_set_credentials(const char *ssid, const char *pass) {
    nvs_storage_add_network(ssid, pass);
}

bool wifi_manager_has_credentials(void) {
    wifi_network_t tmp[1];
    return nvs_storage_load_networks(tmp, 1) > 0;
}

void wifi_manager_load_saved_credentials(void) {
    /* No-op — networks are read on-demand from NVS. */
}

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
 * Public: status getters
 * ============================================================ */

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

void wifi_manager_get_progress(int *current, int *total) {
    /* Read directly — only the worker task ever writes these, and this
     * is called from other tasks for display purposes only (a stale
     * read by a tick or two is harmless). */
    int idx = candidate_index;
    int cnt = candidate_count;
    if (cnt <= 0) cnt = 1;
    if (idx >= cnt) idx = cnt - 1;
    if (idx < 0) idx = 0;
    *current = idx;
    *total   = cnt;
}

esp_netif_t *wifi_manager_get_sta_netif(void) {
    return sta_netif;
}

bool wifi_manager_scan_in_progress(void) {
    return fsm_state == WFSM_CONNECTING; /* scanning happens as part of connecting */
}

int wifi_manager_get_scan_results(wifi_scan_result_t *out, int max_count) {
    int n = scan_result_count;
    if (n > max_count) n = max_count;
    memcpy(out, scan_results, sizeof(wifi_scan_result_t) * n);
    return n;
}

/* ============================================================
 * Public: requests — all just post to the queue. The worker task
 * is the sole owner of WiFi state; nothing else touches it directly.
 * ============================================================ */

static void wifi_post_evt(wifi_evt_type_t type) {
    if (!wifi_evt_queue) return;
    wifi_evt_t evt = { .type = type };
    if (xQueueSend(wifi_evt_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "[WIFI] event queue full, dropping event %d", (int)type);
    }
}

void wifi_manager_start_connect(void) {
    wifi_post_evt(WEVT_CONNECT_REQUEST);
}

void wifi_manager_scan_start(void) {
    wifi_post_evt(WEVT_SCAN_REQUEST);
}

void wifi_manager_clear_credentials(void) {
    wifi_post_evt(WEVT_FORGET_REQUEST);
}

void wifi_manager_schedule_recovery(void) {
    wifi_post_evt(WEVT_RECOVERY_REQUEST);
}

/* ============================================================
 * Scanning helpers — pure functions, no FSM/task state of their own
 * ============================================================ */

static void run_blocking_scan_and_store_results(void) {
    wifi_scan_config_t scan_cfg = {0};
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        scan_result_count = 0;
        return;
    }
    if (ap_count > 32) ap_count = 32;

    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!records) return;

    uint16_t actual = ap_count;
    if (esp_wifi_scan_get_ap_records(&actual, records) != ESP_OK) {
        free(records);
        return;
    }

    wifi_network_t saved[WIFI_MAX_SAVED_NETWORKS];
    int saved_count = nvs_storage_load_networks(saved, WIFI_MAX_SAVED_NETWORKS);

    scan_result_count = 0;
    for (int i = 0; i < actual && scan_result_count < WIFI_MAX_SCAN_RESULTS; i++) {
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

    free(records);
}

static void build_candidate_list(void) {
    wifi_network_t saved[WIFI_MAX_SAVED_NETWORKS];
    int saved_count = nvs_storage_load_networks(saved, WIFI_MAX_SAVED_NETWORKS);

    candidate_count = 0;
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
            candidate_list[candidate_count] = saved[i];
            candidate_rssi[candidate_count] = best_rssi;
            candidate_count++;
        }
    }

    /* Insertion sort by descending RSSI — candidate_count <= 6 */
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

/* ============================================================
 * Connect helpers
 * ============================================================ */

static void connect_to_candidate(int idx) {
    if (candidate_attempt == 0) {
        ESP_LOGI(TAG, "[WIFI] trying candidate %d/%d: %s (rssi %d)",
                 idx + 1, candidate_count, candidate_list[idx].ssid, candidate_rssi[idx]);
    } else {
        ESP_LOGI(TAG, "[WIFI] retrying candidate %d/%d: %s (attempt %d/%d)",
                 idx + 1, candidate_count, candidate_list[idx].ssid,
                 candidate_attempt + 1, CAND_MAX_ATTEMPTS);
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

    esp_wifi_connect();
}

/* Starts a fresh scan+connect cycle. Returns the timeout to wait for
 * (only relevant if it immediately fails and enters RETRY_WAIT). */
static void start_connect_cycle(void) {
    app_state_set(APP_WIFI_CONNECTING);

    run_blocking_scan_and_store_results();
    build_candidate_list();
    candidate_attempt = 0;

    if (candidate_count == 0) {
        ESP_LOGW(TAG, "[WIFI] no saved networks visible, will retry in %d s",
                 WIFI_FULL_RETRY_DELAY_MS / 1000);
        app_state_set(APP_WIFI_FAILED);
        fsm_state = WFSM_RETRY_WAIT;
        retry_reason = RETRY_REASON_INITIAL_FAILED;
        return;
    }

    fsm_state = WFSM_CONNECTING;
    connect_to_candidate(candidate_index);
}

/* ============================================================
 * Worker task — the single owner of all WiFi decision-making
 * ============================================================ */

static uint32_t next_wait_ms = 0; /* 0 means "wait forever" */

static TickType_t wait_ticks(void) {
    return next_wait_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(next_wait_ms);
}

static void handle_timeout(void) {
    switch (fsm_state) {
        case WFSM_RETRY_WAIT:
            if (!wifi_manager_has_credentials()) {
                fsm_state = WFSM_IDLE;
                next_wait_ms = 0;
                return;
            }
            ESP_LOGI(TAG, "[WIFI] retry timer fired, starting a new connect cycle");
            start_connect_cycle();
            break;
        default:
            /* Shouldn't normally happen — no timeout should be armed
             * outside RETRY_WAIT. */
            break;
    }
}

static void handle_event(const wifi_evt_t *evt) {
    switch (fsm_state) {

        case WFSM_IDLE:
            switch (evt->type) {
                case WEVT_CONNECT_REQUEST:
                    if (wifi_manager_has_credentials()) start_connect_cycle();
                    else app_state_set(APP_NO_WIFI);
                    break;
                case WEVT_SCAN_REQUEST:
                    run_blocking_scan_and_store_results();
                    break;
                case WEVT_FORGET_REQUEST:
                    nvs_storage_clear_networks();
                    esp_wifi_disconnect();
                    app_state_set(APP_NO_WIFI);
                    break;
                case WEVT_RECOVERY_REQUEST:
                    /* Nothing active to recover. */
                    break;
                default:
                    break; /* stray DISCONNECTED/GOT_IP — ignore */
            }
            break;

        case WFSM_CONNECTING:
            switch (evt->type) {
                case WEVT_GOT_IP: {
                    taskENTER_CRITICAL(&wifi_mux);
                    snprintf(wifi_ip, sizeof(wifi_ip), IPSTR, IP2STR(&evt->ip));
                    taskEXIT_CRITICAL(&wifi_mux);

                    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
                    wifi_ap_record_t ap = {0};
                    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) set_wifi_rssi(ap.rssi);

                    set_wifi_connected(true);

                    retry_delay_ms = WIFI_RETRY_BASE_MS;
                    fsm_state = WFSM_CONNECTED;
                    next_wait_ms = 0;

                    bool bt = get_bt_connected();
                    if (bt) app_state_set(APP_BRIDGE);
                    else if (app_state_get() != APP_WAIT_BT) app_state_set(APP_WAIT_BT);
                    break;
                }
                case WEVT_DISCONNECTED:
                    candidate_attempt++;
                    if (candidate_attempt < CAND_MAX_ATTEMPTS) {
                        connect_to_candidate(candidate_index);
                    } else {
                        candidate_attempt = 0;
                        candidate_index++;
                        if (candidate_index < candidate_count) {
                            connect_to_candidate(candidate_index);
                        } else {
                            ESP_LOGW(TAG, "[WIFI] exhausted all %d candidate(s), will retry in %d s",
                                     candidate_count, WIFI_FULL_RETRY_DELAY_MS / 1000);
                            app_state_set(APP_WIFI_FAILED);
                            candidate_count = 0;
                            fsm_state = WFSM_RETRY_WAIT;
                            retry_reason = RETRY_REASON_INITIAL_FAILED;
                            next_wait_ms = WIFI_FULL_RETRY_DELAY_MS;
                        }
                    }
                    break;
                case WEVT_FORGET_REQUEST:
                    nvs_storage_clear_networks();
                    esp_wifi_disconnect();
                    app_state_set(APP_NO_WIFI);
                    fsm_state = WFSM_IDLE;
                    next_wait_ms = 0;
                    break;
                default:
                    break; /* duplicate CONNECT/SCAN/RECOVERY while busy — ignore */
            }
            break;

        case WFSM_CONNECTED:
            switch (evt->type) {
                case WEVT_DISCONNECTED: {
                    set_wifi_connected(false);

                    taskENTER_CRITICAL(&wifi_mux);
                    strncpy(wifi_ip, "--", sizeof(wifi_ip));
                    taskEXIT_CRITICAL(&wifi_mux);

                    bool has_ssid = wifi_manager_has_credentials();
                    if (!has_ssid) {
                        app_state_set(APP_NO_WIFI);
                        fsm_state = WFSM_IDLE;
                        next_wait_ms = 0;
                        break;
                    }

                    if (app_state_get() == APP_BRIDGE) {
                        app_state_set(APP_BRIDGE_NO_WIFI);
                    }
                    retry_reason = RETRY_REASON_LOST_CONNECTION;
                    fsm_state = WFSM_RETRY_WAIT;
                    next_wait_ms = retry_delay_ms;
                    retry_delay_ms = (retry_delay_ms * 2 > WIFI_RETRY_MAX_MS)
                                    ? WIFI_RETRY_MAX_MS : retry_delay_ms * 2;
                    break;
                }
                case WEVT_SCAN_REQUEST:
                    run_blocking_scan_and_store_results();
                    break;
                case WEVT_FORGET_REQUEST:
                    nvs_storage_clear_networks();
                    esp_wifi_disconnect();
                    /* WEVT_DISCONNECTED will follow and drive the state
                     * transition to IDLE/NO_WIFI. */
                    break;
                case WEVT_RECOVERY_REQUEST:
                    ESP_LOGW(TAG, "[WIFI] recovery requested while connected — resetting");
                    fsm_state = WFSM_RECOVERING;
                    wifi_soft_reset();
                    fsm_state = WFSM_CONNECTING;
                    next_wait_ms = 0;
                    break;
                default:
                    break; /* CONNECT_REQUEST while already connected — ignore */
            }
            break;

        case WFSM_RETRY_WAIT:
            switch (evt->type) {
                case WEVT_CONNECT_REQUEST:
                    ESP_LOGI(TAG, "[WIFI] connect requested, skipping remaining retry wait");
                    start_connect_cycle();
                    break;
                case WEVT_FORGET_REQUEST:
                    nvs_storage_clear_networks();
                    app_state_set(APP_NO_WIFI);
                    fsm_state = WFSM_IDLE;
                    next_wait_ms = 0;
                    break;
                case WEVT_RECOVERY_REQUEST:
                    fsm_state = WFSM_RECOVERING;
                    wifi_soft_reset();
                    fsm_state = WFSM_CONNECTING;
                    next_wait_ms = 0;
                    break;
                default:
                    break; /* stray DISCONNECTED/GOT_IP from a finished cycle — ignore */
            }
            break;

        case WFSM_RECOVERING:
            /* wifi_soft_reset() runs synchronously; this state is never
             * actually observed by the event loop. */
            break;
    }
}

static void wifi_worker_task(void *arg) {
    (void)arg;
    wifi_evt_t evt;

    while (1) {
        BaseType_t got = xQueueReceive(wifi_evt_queue, &evt, wait_ticks());
        if (got == pdTRUE) {
            handle_event(&evt);
        } else {
            handle_timeout();
        }
    }
}

/* ============================================================
 * esp_event handler — only translates IDF events into our own
 * queue events. No decision-making happens here.
 * ============================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_post_evt(WEVT_DISCONNECTED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        wifi_evt_t evt = { .type = WEVT_GOT_IP, .ip = e->ip_info.ip };
        if (wifi_evt_queue) xQueueSend(wifi_evt_queue, &evt, 0);
    }
}

/* ============================================================
 * Init / soft reset
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
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt_inst);
        wifi_evt_inst = NULL;
    }
    if (ip_evt_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_evt_inst);
        ip_evt_inst = NULL;
    }
}

/* Runs synchronously inside the worker task — blocking here is fine,
 * nothing else can meaningfully happen to WiFi state while this runs. */
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

void wifi_manager_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_evt_queue = xQueueCreate(8, sizeof(wifi_evt_t));
    safe_task_create(wifi_worker_task, "wifi_worker", 4096, NULL, 5, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_register_handlers();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}
