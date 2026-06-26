// btstack_config.h for Satura Bridge (ESP32 PAN WiFi Bridge)

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_ASSERT
#define HAVE_BTSTACK_STDIN
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_FREERTOS_INCLUDE_PREFIX
#define HAVE_FREERTOS_TASK_NOTIFICATIONS
#define HAVE_MALLOC

// ESP32 has VHCI
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL

// Logging
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP

// ESP32 is dual-mode, но нам нужен только CLASSIC дл€ PAN
#define ENABLE_BLE
#define ENABLE_CLASSIC

// === CLASSIC (об€зательно дл€ PAN) ===
#ifdef ENABLE_CLASSIC

// BNEP/PAN support
#define ENABLE_BNEP
#define ENABLE_L2CAP

// ACL buffer дл€ Ethernet frames в BNEP (1500 + заголовки)
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)

#define HCI_HOST_ACL_PACKET_LEN 1024
#define HCI_HOST_ACL_PACKET_NUM 20
#define HCI_HOST_SCO_PACKET_LEN 60
#define HCI_HOST_SCO_PACKET_NUM 10

// ƒл€ старых телефонов (Nokia и т.д.)
#define ENABLE_CLASSIC_LEGACY_CONNECTIONS_FOR_SCO_DEMOS

// Link keys
#define NVM_NUM_LINK_KEYS 4

#endif

// === LE (не критично, но оставим) ===
#ifdef ENABLE_BLE
#define ENABLE_ATT_DELAYED_RESPONSE
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define NVM_NUM_DEVICE_DB_ENTRIES 8
#endif

#endif