idf_component_register(
    SRCS
        "app_bootstrap.c"
        "main.c"
        "dns_server.c"
        "nvs_storage.c"
        "http_utils.c"
        "http_routes.c"
        "proxy_relay.c"
        "app_state.c"
        "wifi_manager.c"
        "bt_pan.c"
        "uptime.c"
        "http_server.c"
        "nat_bridge.c"
        "proxy_gateway.c"
        "event_bus.c"
        "storage.c"
        "watchdog.c"

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
