

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// BTstack config for ESP32

#define HAVE_INIT_SCRIPT
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_BTSTACK_STDIN

// Bluetooth features
#define ENABLE_CLASSIC
//#define ENABLE_BLE
//#define ENABLE_LE_PERIPHERAL
//#define ENABLE_LE_CENTRAL

// PAN / BNEP
#define ENABLE_BNEP
#define ENABLE_L2CAP

// Security
#define ENABLE_SSP
//#define ENABLE_LE_SECURE_CONNECTIONS

// Memory
// Увеличиваем размер ACL-пакетов до максимума для классического BT
#define HCI_ACL_PAYLOAD_SIZE 1691

// Явно задаём MTU для L2CAP (будет использоваться при создании каналов)
//#define L2CAP_DEFAULT_MTU 1691

// Можно также увеличить количество каналов (на всякий случай)
#define MAX_NR_L2CAP_CHANNELS  4   // было 2
#define MAX_NR_BNEP_CHANNELS   2   // было 1

#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 2
#define MAX_NR_L2CAP_SERVICES  2

#define MAX_NR_BNEP_SERVICES 1

#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 2

// SDP
#define MAX_NR_SDP_RECORDS 1
#define MAX_NR_SDP_CLIENT_CONNECTIONS 1

// Run loop
#define ENABLE_BTSTACK_ASSERT
#define HAVE_FREERTOS_TASK_NOTIFICATIONS
// Привязать BTstack run loop к конкретному ядру
#define BTSTACK_RUN_LOOP_FREERTOS_TASK_CORE 0

// Debug (можно отключить потом)
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_LOG_DEBUG

#endif