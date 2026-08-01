idf_component_register(
    SRCS
        "pan_wifi_bridge.c"
        "main.c"
        "dns_server.c"
        "nvs_storage.c"
        "http_utils.c"

    INCLUDE_DIRS "."

    REQUIRES
        btstack-esp32
        esp_wifi
        esp_netif
        esp_http_server
        nvs_flash
        lwip
        freertos
        esp_timer
        esp_coex
)