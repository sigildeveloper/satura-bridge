# Exactly one board backend is compiled in, selected via Kconfig
# (menu "Satura Bridge" -> "Hardware board", see main/Kconfig).
# Application code never changes based on this choice — only
# main/board/ does. See main/board/board.h for the interface.
if(CONFIG_SATURA_BOARD_CORE2)
    set(BOARD_SRCS "board/board_core2.c")
elseif(CONFIG_SATURA_BOARD_STICKC_PLUS2)
    set(BOARD_SRCS "board/board_stickc_plus2.c")
else()
    set(BOARD_SRCS "board/board_generic.c")
endif()

idf_component_register(
    SRCS
        "app_bootstrap.c"
        "main.c"
        "dns_server.c"
        "nvs_storage.c"
        "http_utils.c"
        "http_routes.c"
        "proxy_relay.c"
        "clipboard.c"
        "device_name.c"
        "battery.c"
        "app_state.c"
        "wifi_manager.c"
        "bt_pan.c"
        "uptime.c"
        "http_server.c"
        "nat_bridge.c"
        "link_registry.c"
        "proxy_gateway.c"
        "event_bus.c"
        "storage.c"
        "watchdog.c"
        ${BOARD_SRCS}

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
        driver
        esp_adc
)
