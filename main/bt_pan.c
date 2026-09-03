#include <string.h>
#include "btstack_config.h"
#include "bnep_lwip.h"
#include "btstack.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#include "bt_pan.h"
#include "app_state.h"
#include "event_bus.h"
#include "link_iface.h"
#include "device_name.h"
#include "config.h"
#include "task_utils.h"

static const char *TAG = "bt_pan";

#define BT_LEGACY_PIN       "0000"
#define BT_REOPEN_DELAY_MS  1200

static portMUX_TYPE bt_pan_mux = portMUX_INITIALIZER_UNLOCKED;

static hci_con_handle_t bt_handle = HCI_CON_HANDLE_INVALID;

static volatile bool bt_reopen_running  = false;
static volatile int  bt_reopen_counter  = 0;

static uint8_t pan_sdp_record[400];
static btstack_packet_callback_registration_t hci_event_cb;

static volatile bool rssi_poll_pending = false;

static void bt_reopen_task(void *arg);
static void hci_packet_handler(uint8_t type, uint16_t ch,
                                uint8_t *pkt, uint16_t sz);
static void bnep_lwip_packet_handler(uint8_t type, uint16_t ch,
                                      uint8_t *pkt, uint16_t sz);

/* ============================================================
 * link_iface_t registration — bt_pan is the downlink (client-facing)
 * side. get_netif() has to fall back to scanning netif_list by
 * subnet, the same way nat_bridge.c used to do it directly: bnep_lwip
 * (the third-party btstack ESP32 port) creates its lwIP netif
 * internally and never hands back a pointer to it, so there's no
 * better source. Cached once found — bnep_lwip creates this netif
 * once at boot and keeps reusing it across BT reconnects. A future
 * downlink transport that owns its netif directly (unlike bnep_lwip)
 * doesn't need this workaround at all; it can just return its own
 * stored pointer.
 * ============================================================ */

static struct netif *cached_netif = NULL;

static struct netif *bt_pan_get_netif(void) {
    if (cached_netif) return cached_netif;
    for (struct netif *p = netif_list; p; p = p->next) {
        const ip4_addr_t *ip = netif_ip4_addr(p);
        if (ip4_addr1(ip) == GW_IP0 &&
            ip4_addr2(ip) == GW_IP1 &&
            ip4_addr3(ip) == GW_IP2) {
            cached_netif = p;
            return p;
        }
    }
    return NULL;
}

static const link_iface_t bt_pan_link = {
    .name         = "bt_pan",
    .role         = LINK_ROLE_DOWNLINK,
    .init         = NULL, /* registered explicitly at the end of bt_pan_init() */
    .is_connected = get_bt_connected,
    .get_netif    = bt_pan_get_netif,
};

/* ============================================================
 * Accessors
 * ============================================================ */

static hci_con_handle_t get_bt_handle(void) {
    hci_con_handle_t h;
    taskENTER_CRITICAL(&bt_pan_mux);
    h = bt_handle;
    taskEXIT_CRITICAL(&bt_pan_mux);
    return h;
}

/* ============================================================
 * RSSI polling
 * ============================================================ */

static void rssi_poll_cb(void *context) {
    (void)context;
    hci_con_handle_t h = get_bt_handle();
    if (h != HCI_CON_HANDLE_INVALID) gap_read_rssi(h);
    rssi_poll_pending = false;
}

static btstack_context_callback_registration_t rssi_cb_reg = {
    .callback = rssi_poll_cb,
    .context  = NULL,
};

void bt_pan_request_rssi_poll(void) {
    if (!rssi_poll_pending) {
        rssi_poll_pending = true;
        btstack_run_loop_execute_on_main_thread(&rssi_cb_reg);
    }
}

/* ============================================================
 * Visibility / reopen
 * ============================================================ */

static void bt_set_visible(bool v) {
    gap_discoverable_control(v ? 1 : 0);
    gap_connectable_control(v ? 1 : 0);
    ESP_LOGI(TAG, "[BT] %s", v ? "visible" : "hidden");
}

static void bt_reopen_task(void *arg) {
    (void)arg;
    ESP_LOGW(TAG, "[BTR] started, heap=%u",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    vTaskDelay(pdMS_TO_TICKS(BT_REOPEN_DELAY_MS));

    if (!get_bt_connected()) bt_set_visible(true);

    taskENTER_CRITICAL(&bt_pan_mux);
    bt_reopen_running = false;
    taskEXIT_CRITICAL(&bt_pan_mux);
    vTaskDelete(NULL);
}

/* ============================================================
 * HCI / BNEP handlers
 * ============================================================ */

static void hci_packet_handler(uint8_t type, uint16_t ch,
                                uint8_t *pkt, uint16_t sz) {
    (void)ch; (void)sz;
    if (type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    switch (hci_event_packet_get_type(pkt)) {
        case HCI_EVENT_CONNECTION_REQUEST:
            hci_event_connection_request_get_bd_addr(pkt, addr);
            ESP_LOGI(TAG,
                "[HCI] Connection request from %02X:%02X:%02X:%02X:%02X:%02X",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            break;
        case HCI_EVENT_CONNECTION_COMPLETE:
            hci_event_connection_complete_get_bd_addr(pkt, addr);
            ESP_LOGI(TAG,
                "[HCI] Connection complete status=0x%02x addr=%02X:%02X:%02X:%02X:%02X:%02X",
                hci_event_connection_complete_get_status(pkt),
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            hci_con_handle_t disc_handle =
                hci_event_disconnection_complete_get_connection_handle(pkt);
            ESP_LOGI(TAG,
                "[HCI] Disconnected handle=0x%04x reason=0x%02x",
                disc_handle,
                hci_event_disconnection_complete_get_reason(pkt));

            /* If the phone tore down the HCI link before BNEP ever opened
                * (e.g. it gave up mid-handshake), BNEP_EVENT_CHANNEL_CLOSED
                * never fires and we'd stay hidden forever. Make sure we
                * become visible again if we're not actually bridging. */
            if (!get_bt_connected() && disc_handle == get_bt_handle()) {
                taskENTER_CRITICAL(&bt_pan_mux);
                bt_handle = HCI_CON_HANDLE_INVALID;
                bool already = bt_reopen_running;
                if (!already) bt_reopen_running = true;
                taskEXIT_CRITICAL(&bt_pan_mux);

                if (!already) {
                    ESP_LOGW(TAG, "[BTR] HCI disconnect without BNEP open, recovering visibility");
                    safe_task_create(bt_reopen_task, "btr", 4096, NULL, 4, NULL);
                }
            }
            break;
        }
        case GAP_EVENT_RSSI_MEASUREMENT:
            if (gap_event_rssi_measurement_get_con_handle(pkt) == get_bt_handle()) {
                int8_t rssi = gap_event_rssi_measurement_get_rssi(pkt);
                /* 0 dBm from a fresh/not-yet-ready connection reads as an
                    * implausibly perfect signal — treat it as "no data yet"
                    * rather than overwriting a real prior reading with it. */
                if (rssi != 0) {
                    set_bt_rssi(rssi);
                }
            }
            break;
        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(pkt, addr);
            gap_pin_code_response(addr, BT_LEGACY_PIN);
            break;
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            hci_event_user_confirmation_request_get_bd_addr(pkt, addr);
            gap_ssp_confirmation_response(addr);
            break;
        default: break;
    }
}

static void bnep_lwip_packet_handler(uint8_t type, uint16_t ch,
                                      uint8_t *pkt, uint16_t sz) {
    (void)ch; (void)sz;
    if (type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    switch (hci_event_packet_get_type(pkt)) {

        case BNEP_EVENT_CHANNEL_OPENED: {
            if (bnep_event_channel_opened_get_status(pkt)) {
                ESP_LOGI(TAG, "[BT] BNEP open failed status=0x%02x",
                         bnep_event_channel_opened_get_status(pkt));
                break;
            }
            bnep_event_channel_opened_get_remote_address(pkt, addr);
            hci_con_handle_t h = bnep_event_channel_opened_get_con_handle(pkt);

            if (get_bt_connected()) {
                bnep_disconnect(addr);
                return;
            }
            set_bt_connected(true);
            event_bus_publish(EVENT_DOWNLINK_UP);

            taskENTER_CRITICAL(&bt_pan_mux);
            bt_handle = h;
            taskEXIT_CRITICAL(&bt_pan_mux);

            ESP_LOGI(TAG, "[BT] BNEP channel opened");
            bt_set_visible(false);

            /* set_bt_connected() above published EVENT_BT_CONNECTED.
             * wifi_manager owns the decision of whether/when to start
             * a WiFi connect attempt from here — see on_bt_event() in
             * wifi_manager.c. We only report app state that is already
             * known to us here. */
            if (get_wifi_connected()) {
                app_state_set(APP_BRIDGE);
            }
            break;
        }

        case BNEP_EVENT_CHANNEL_CLOSED: {
            set_bt_connected(false);
            event_bus_publish(EVENT_DOWNLINK_DOWN);
            set_bt_rssi(-100);

            taskENTER_CRITICAL(&bt_pan_mux);
            bt_handle    = HCI_CON_HANDLE_INVALID;
            bool already  = bt_reopen_running;
            if (!already) bt_reopen_running = true;
            taskEXIT_CRITICAL(&bt_pan_mux);

            app_state_set(APP_WAIT_BT);

            if (!already) {
                bt_reopen_counter++;
                ESP_LOGW(TAG, "[BTR] create #%d heap=%u",
                         bt_reopen_counter,
                         heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
                safe_task_create(bt_reopen_task, "btr", 4096, NULL, 4, NULL);
            } else {
                ESP_LOGW(TAG, "[BTR] already running, skip create");
            }

            break;
        }

        default: break;
    }
}

/* ============================================================
 * Setup / start
 * ============================================================ */

void bt_pan_init(void) {
    gap_set_local_name(device_name_get());
    gap_discoverable_control(1);
    gap_connectable_control(1);
    gap_set_class_of_device(0x020302);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_set_security_level(LEVEL_0);

    hci_event_cb.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_cb);

#if defined(L2CAP_SET_MAX_MTU)
    l2cap_set_max_mtu(1691);
#endif
    l2cap_init();

    bnep_init();
    sdp_init();

    memset(pan_sdp_record, 0, sizeof(pan_sdp_record));
    uint16_t net_types[] = {0x0800, 0x0806, 0};
    pan_create_nap_sdp_record(pan_sdp_record,
                              sdp_create_service_record_handle(),
                              net_types, NULL, NULL, BNEP_SECURITY_NONE,
                              PAN_NET_ACCESS_TYPE_OTHER, 128000,
                              device_name_get(), "BT PAN WiFi Bridge");
    sdp_register_service(pan_sdp_record);

    bnep_lwip_init();
    bnep_lwip_register_service(BLUETOOTH_SERVICE_CLASS_NAP, 1691);
    bnep_lwip_register_packet_handler(bnep_lwip_packet_handler);

    link_registry_register(&bt_pan_link);
}

void bt_pan_start(void) {
    hci_power_control(HCI_POWER_ON);
}
