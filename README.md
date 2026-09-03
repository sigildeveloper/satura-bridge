# Satura Bridge

**«Bringing old phones back online.»**

Open-source Bluetooth Classic PAN → Internet gateway for legacy mobile phones and Symbian devices, currently using Wi-Fi as the uplink.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.15-green.svg)](https://github.com/sigildeveloper/satura-bridge/releases/tag/v0.0.15)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

## What is Satura Bridge?

Satura Bridge is a small ESP32-based Bluetooth Classic PAN gateway for legacy mobile phones.

The phone connects through Bluetooth PAN, while the ESP32 currently uses Wi-Fi as its Internet uplink and performs NAT between the two links.

Many older Nokia Symbian, Sony Ericsson and other legacy devices have Bluetooth but no Wi-Fi. Satura Bridge provides Internet access without requiring special phone software when the device supports the required Bluetooth networking profile.

```text
┌──────────────────┐
│   Legacy phone   │
│  Nokia / Symbian │
└────────┬─────────┘
         │ Bluetooth Classic / PAN
         ▼
┌──────────────────┐
│  Satura Bridge   │
│      ESP32       │
└────────┬─────────┘
         │ Wi-Fi uplink
         ▼
┌──────────────────┐
│      Router      │
└────────┬─────────┘
         ▼
       Internet
```

> **Current transport support:** Bluetooth Classic PAN as downlink and Wi-Fi as uplink. The transport abstraction is ready for additional implementations, but Bluetooth PPP, Ethernet, Serial/IR and cellular modem transports are not yet user-facing features.

> **Compatibility:** Windows 10 Internet connectivity is currently not supported. Older Android devices, Sony Ericsson J108, NetFront 3.4 and J2ME applications have been tested successfully.

---

## Features

* Bluetooth Classic PAN / NAP
* Wi-Fi Internet uplink
* Transport-agnostic network interface abstraction
* NAT / routing between active links
* Event bus for decoupled link-state notifications
* Web-based configuration
* Captive-portal-style setup
* Up to 6 saved Wi-Fi networks
* Wi-Fi scanning and signal-based network selection
* Automatic connection recovery and failover
* Watchdog and component recovery
* HTTP proxy gateway
* HTTP POST body forwarding through the proxy
* Proxy hostname resolution with cached IPv4 address
* Legacy-browser-friendly HTTP/1.0 proxy response framing
* DNS forwarding with upstream source verification
* HTTP worker stack high-water-mark monitoring
* Explicit `-Os` size optimization
* Single-file firmware flashing

---

## HTTP Proxy Gateway

Configure the optional HTTP proxy at `/proxy`.

The gateway can be specified using either an IPv4 address or a hostname.

Hostnames are resolved when the proxy configuration is loaded or changed, and the resulting IPv4 address is cached.

The proxy relay supports HTTP requests including POST bodies.

For compatibility with older browsers and WAP software, proxy responses use HTTP/1.0-style framing with `Content-Length` or connection-close semantics instead of HTTP/1.1 chunked encoding.

### Example gateway: 15pmm01.com

[15pmm01.com](https://15pmm01.com/wap/en/) is a public WAP compression gateway that works well as a proxy target for old browsers — it re-compresses and simplifies pages on the fly, which helps a lot on the tiny screens and slow links this bridge is built for. It's built and maintained by [@petarmarinov37](https://t.me/petarmarinov37) on Telegram, who also runs other proxy servers for the retro-phone community.

---

# Architecture

Starting with v0.0.15, the networking core is transport-agnostic.

```text
                 ┌──────────────────────┐
                 │      HTTP / DNS      │
                 │   configuration      │
                 └──────────┬───────────┘
                            │
                 ┌──────────▼────────────┐
                 │      nat_bridge       │
                 │ transport-independent │
                 └──────────┬────────────┘
                            │
                  link_registry / link_iface
                       │                │
                    DOWNLINK          UPLINK
                       │                │
                  ┌────▼────┐      ┌────▼────┐
                  │ bt_pan  │      │  Wi-Fi  │
                  └─────────┘      └─────────┘
```

### `link_iface`

`link_iface_t` provides a generic interface for a network link.

The networking core does not need to know whether the underlying link is Bluetooth, Wi-Fi, Ethernet, a cellular modem or another transport.

### `link_registry`

`link_registry` keeps track of the currently active link for each role:

* `DOWNLINK`
* `UPLINK`

### `nat_bridge`

`nat_bridge` performs the bridge/NAT functionality through the generic link interface instead of depending directly on Bluetooth PAN or Wi-Fi.

This makes it possible to add new transports without rewriting the NAT, HTTP or DNS layers.

### Event bus

Generic link state events are available:

* `EVENT_DOWNLINK_UP`
* `EVENT_DOWNLINK_DOWN`
* `EVENT_UPLINK_UP`
* `EVENT_UPLINK_DOWN`

Transport-specific modules can publish these events alongside their own transport-specific events.

### Future transports

The architecture is intended to allow implementations such as:

* Bluetooth PPP
* Serial / IR
* Ethernet
* Cellular / 4G / 5G modem
* Other network interfaces

These are **not currently implemented as user-facing transports**.

---

# v0.0.15

v0.0.15 is primarily an **architecture, reliability and hardening release**.

## Transport abstraction

* Added `link_iface_t`.
* Added `link_registry`.
* `nat_bridge` no longer depends directly on BT PAN or Wi-Fi.
* Added generic downlink/uplink link-state events.
* Added the foundation for future Bluetooth PPP, Serial/IR, Ethernet and cellular modem transports.
* `bt_pan` now reacts to `EVENT_BT_CONNECTED` instead of directly calling `wifi_manager`.

## Reliability and security

* DNS replies are accepted only when their source IP and port match the configured upstream DNS server.
* Proxy relay no longer relies on HTTP/1.1 chunked response framing for legacy clients.
* Proxy responses use HTTP/1.0-compatible framing with `Content-Length` or connection-close semantics.
* Fixed `EVENT_TYPE_COUNT`, which could silently drop events added beyond the previous hardcoded count.

## Wi-Fi

* Wi-Fi manager now uses a single-owner queue-based state machine.
* Connect, scan, retry and recovery operations are handled through the same state machine.
* Improved connection recovery and failover behavior.

## Code organization

* NVS access is centralized behind the storage abstraction.
* `http_server.c` was split into:
  * `http_routes.c`
  * `proxy_relay.c`
* The remaining `http_server.c` handles server startup and URI registration.
* `pan_wifi_bridge.c` became `app_bootstrap.c`.

## Memory and build optimization

* `-Os` is explicitly enabled.
* Measured IRAM usage decreased from approximately **79.7% to 74.31%**.
* Firmware image size decreased by approximately **65 KB**.
* Added HTTP worker stack high-water-mark monitoring.

## Hardware testing

v0.0.15 was tested on a Sony Ericsson J108 with:

* Bluetooth PAN connection
* Bluetooth PAN reconnection
* Wi-Fi failover
* NAT following independent Bluetooth/Wi-Fi link state changes
* HTTP proxy relay traffic to a real external website
* HTML, CSS and image loading through the proxy

[Release v0.0.15](https://github.com/sigildeveloper/satura-bridge/releases/tag/v0.0.15) includes the prebuilt `satura-bridge-v0.0.15.bin` firmware.

---

# Hardware

| Component | Recommended | Minimum |
| --------- | ----------- | ------- |
| Board | M5Stack Core2 | ESP32-WROOM-32 |
| Power | Built-in battery | Any USB 5V supply |

The current development hardware uses ESP32-D0WDQ6-V3.

Satura Bridge requires an ESP32 variant with Bluetooth Classic support.

Bluetooth and Wi-Fi operate simultaneously and share ESP32 RF resources, so antenna design and placement can affect stability and performance.

---

# Quick Start

## Flash

See [FLASH.md](FLASH.md).

The latest firmware is:

```text
firmware/satura-bridge-v0.0.15.bin
```

A browser-based flashing option is also available through ESP Web Tools.

Alternatively:

```bash
esptool --port COM3 --baud 460800 write-flash 0x0 firmware/satura-bridge-v0.0.15.bin
```

## Connect a phone

1. Enable Bluetooth.
2. Find **Satura Bridge**.
3. Pair with the device.
4. If a PIN is requested, use `0000`.
5. Open the phone browser.
6. Open `http://192.168.7.1` or any HTTP URL.
7. Configure Wi-Fi.

Saved Wi-Fi settings survive reboot and the device automatically attempts to reconnect.

---

# Web Interface

The web interface is available at:

**http://192.168.7.1**

| Page | Description |
| ---- | ----------- |
| `/` | Status, RSSI, uptime and heap information |
| `/setup` | Manual Wi-Fi configuration |
| `/networks` | Scan and manage saved Wi-Fi networks |
| `/proxy` | Configure HTTP proxy gateway |
| `/reset` | Remove all saved Wi-Fi networks |
| `/reboot` | Restart the device |

---

# Performance

Typical measured values:

| Parameter | Value |
| --------- | ----- |
| Download | ~0.15–0.20 Mbit/s |
| Upload | ~0.20–0.27 Mbit/s |
| Ping | 150–200 ms |
| Maximum Bluetooth PAN clients | 1 |
| Power draw | ~200 mA from USB |

Actual performance depends on RF conditions and the Bluetooth PAN implementation.

The available performance is normally sufficient for:

* retro browsers
* lightweight web pages
* messaging
* J2ME applications
* WAP-related software

---

# Compatibility

| Device / Platform | Result |
| ----------------- | ------ |
| Sony Ericsson J108 | Tested |
| Older Android devices | Working |
| NetFront 3.4 | Working |
| J2ME / Opera Mini | Working |
| Built-in email client | Working |

Windows 10 Internet connectivity is currently **not supported**.

For compatibility reports, include:

* Device model
* Operating system/version
* Bluetooth profile support
* Pairing result
* PAN connection result
* Whether web browsing worked

Community:

https://t.me/nnmidletschat

---

# Roadmap

* [x] Bluetooth Classic PAN
* [x] Wi-Fi Internet connectivity
* [x] NAT / routing
* [x] Web configuration
* [x] Multiple saved Wi-Fi networks
* [x] Wi-Fi scanning
* [x] Connection recovery
* [x] Watchdog
* [x] Event bus
* [x] Storage abstraction
* [x] HTTP proxy gateway
* [x] Proxy POST body forwarding
* [x] Proxy hostname resolution
* [x] Transport-agnostic link abstraction
* [ ] Bluetooth PPP downlink
* [ ] Serial / IR downlink
* [ ] Ethernet uplink
* [ ] Cellular modem uplink
* [ ] Test more Nokia Symbian devices
* [ ] Test more Sony Ericsson devices
* [ ] Improve compatibility with older phones
* [ ] Improve Bluetooth / Wi-Fi coexistence
* [ ] Compact custom PCB

---

# Building from Source

Requires [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

For a clean rebuild after changing SDK configuration:

```bash
idf.py fullclean
idf.py build
```

---

# Project Structure

```text
satura-bridge/
├── main/
│   ├── main.c
│   ├── app_bootstrap.c/.h     # Application startup/core coordination
│   ├── app_state.c/.h         # Runtime bridge state
│   ├── bt_pan.c/.h            # Bluetooth PAN / BNEP transport
│   ├── wifi_manager.c/.h      # Wi-Fi state machine/recovery
│   ├── link_iface.c/.h        # Generic network interface
│   ├── link_registry.c/.h     # Active downlink/uplink registry
│   ├── nat_bridge.c/.h        # Transport-independent NAT
│   ├── event_bus.c/.h         # Generic event bus
│   ├── storage.c/.h           # Persistent storage abstraction
│   ├── proxy_gateway.c/.h     # Proxy configuration/resolution
│   ├── proxy_relay.c/.h       # Raw-socket proxy relay
│   ├── dns_server.c/.h        # DNS forwarding/cache/captive replies
│   ├── http_server.c/.h       # HTTP server startup/URI registration
│   ├── http_routes.c/.h       # Web UI/configuration handlers
│   ├── http_utils.c/.h        # HTTP helpers
│   ├── watchdog.c/.h          # Watchdog/recovery
│   ├── uptime.c/.h            # Uptime tracking
│   ├── config.h               # Shared constants
│   └── btstack_config.h       # BTstack configuration
├── components/                # BTstack and dependencies
├── firmware/
│   └── satura-bridge-v0.0.15.bin
├── sdkconfig.defaults
├── FLASH.md
├── CHANGELOG.md
└── README.md
```

---

# Related Projects

## [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge provides Bluetooth PPP Internet connectivity for older S60 v1 devices, including Nokia N-Gage, which do not support Bluetooth PAN.

It uses GnuBox on the phone.

Satura Bridge and Vetera Bridge target different generations of legacy mobile phones and use different networking technologies.

Built and maintained by [@arifwn](https://github.com/arifwn) on GitHub.

---

# License

Satura Bridge is released under the MIT License.

You can use and modify the project as you want. Keep the original attribution.

BTstack in `components/btstack/` is distributed under its own license.

See:

[`components/btstack/LICENSE`](components/btstack/LICENSE)

---

# Author & Community

**Author:** [@sigdev](https://github.com/sigildeveloper)

**Community:** [Telegram — @nnmidletschat](https://t.me/nnmidletschat)

**Vintage phones • Symbian • J2ME • Retro networking**

---

# Русская версия

**«Возвращаем старые телефоны в интернет.»**

Satura Bridge — открытый Bluetooth Classic PAN → Internet шлюз на ESP32 для старых мобильных телефонов и устройств Symbian.

В текущей версии Bluetooth PAN используется как downlink, а Wi-Fi — как uplink.

---

## Что такое Satura Bridge?

Satura Bridge позволяет старым телефонам с Bluetooth получать доступ в интернет через Wi-Fi.

```text
┌──────────────────┐
│  Старый телефон  │
│  Nokia / Symbian │
└────────┬─────────┘
         │ Bluetooth Classic / PAN
         ▼
┌──────────────────┐
│  Satura Bridge   │
│      ESP32       │
└────────┬─────────┘
         │ Wi-Fi
         ▼
┌──────────────────┐
│      Роутер      │
└────────┬─────────┘
         ▼
       Интернет
```

Телефону не требуется специальное программное обеспечение, если он поддерживает необходимый Bluetooth PAN-профиль.

> **Важно:** Windows 10 в текущей версии не поддерживается. Старые Android-устройства, Sony Ericsson J108, NetFront 3.4 и J2ME-приложения были протестированы.

---

## Архитектура v0.0.15

В v0.0.15 сетевое ядро стало независимым от конкретного транспорта.

```text
                  ┌─────────────────┐
                  │    HTTP / DNS   │
                  └────────┬────────┘
                           │
                  ┌────────▼────────┐
                  │   nat_bridge    │
                  └────────┬────────┘
                           │
                    link_registry
                       │       │
                  DOWNLINK   UPLINK
                       │       │
                    BT PAN    Wi-Fi
```

### `link_iface_t`

Универсальный интерфейс сетевого соединения.

### `link_registry`

Хранит активные интерфейсы для ролей:

* `DOWNLINK`
* `UPLINK`

### `nat_bridge`

Работает через абстрактный интерфейс и больше не зависит непосредственно от Bluetooth PAN или Wi-Fi.

Это позволяет в будущем добавлять другие транспорты без переписывания NAT, HTTP и DNS.

Планируемые варианты:

* Bluetooth PPP
* Serial / IR
* Ethernet
* 4G / 5G modem
* другие сетевые интерфейсы

Пока из них реально реализованы только:

* Bluetooth Classic PAN
* Wi-Fi

---

## Возможности

* Bluetooth Classic PAN / NAP
* Wi-Fi uplink
* Transport-agnostic network interface
* NAT / маршрутизация
* Event bus
* Generic link-state events
* Веб-интерфейс
* Captive portal
* До 6 сохранённых Wi-Fi сетей
* Сканирование Wi-Fi
* Автоматический выбор сети
* Recovery / failover
* Watchdog
* HTTP proxy gateway
* POST body forwarding
* Разрешение hostname proxy gateway
* Кеширование IPv4 proxy gateway
* HTTP/1.0 framing для старых клиентов
* Проверка источника DNS-ответов
* HTTP worker stack monitoring
* `-Os` optimization

---

## HTTP Proxy

Прокси настраивается через:

```text
http://192.168.7.1/proxy
```

Gateway можно указать:

```text
192.168.1.100
```

или:

```text
proxy.example.com
```

Hostname разрешается и полученный IPv4-адрес кешируется.

Proxy relay поддерживает POST body.

Для совместимости со старыми браузерами и WAP-программами используется HTTP/1.0-style framing с `Content-Length` или закрытием соединения вместо HTTP/1.1 chunked encoding.

### Пример шлюза: 15pmm01.com

[15pmm01.com](https://15pmm01.com/wap/en/) — публичный WAP-компрессирующий шлюз, хорошо подходящий как значение для этого поля: он на лету пережимает и упрощает страницы, что заметно помогает на маленьких экранах и медленных каналах, под которые и делается этот бридж. Разрабатывает и поддерживает его [@petarmarinov37](https://t.me/petarmarinov37) в Telegram — он же держит и другие прокси-серверы для сообщества любителей ретро-телефонов.

---

## v0.0.15 — что изменилось

v0.0.15 — в первую очередь релиз **архитектурного рефакторинга, оптимизации и повышения надёжности**.

### Transport abstraction

* Добавлен `link_iface_t`.
* Добавлен `link_registry`.
* `nat_bridge` отвязан от BT PAN и Wi-Fi.
* Добавлены generic events:
  * `EVENT_DOWNLINK_UP`
  * `EVENT_DOWNLINK_DOWN`
  * `EVENT_UPLINK_UP`
  * `EVENT_UPLINK_DOWN`
* Создана основа для Bluetooth PPP, Serial/IR, Ethernet и cellular modem.
* `bt_pan` теперь реагирует на `EVENT_BT_CONNECTED`, а не вызывает `wifi_manager` напрямую.

### Wi-Fi

* Wi-Fi manager переведён на queue-based state machine.
* Connect, scan, retry и recovery используют единого владельца очереди.
* Улучшено восстановление соединения.

### DNS и proxy

* DNS теперь принимает ответы только от настроенного upstream DNS-сервера.
* Проверяются IP и порт источника DNS-ответа.
* Proxy relay больше не использует HTTP/1.1 chunked framing для legacy clients.
* Используется HTTP/1.0-compatible framing.
* Добавлена поддержка POST request bodies.
* Proxy gateway может задаваться hostname.
* IPv4-результат hostname кешируется.

### Структура проекта

* NVS доступ централизован через storage abstraction.
* `http_server.c` разделён на:
  * `http_routes.c`
  * `proxy_relay.c`
* `pan_wifi_bridge.c` переименован в `app_bootstrap.c`.

### Оптимизация

* Явно зафиксирован `-Os`.
* IRAM usage уменьшен примерно с **79.7% до 74.31%**.
* Размер firmware уменьшен примерно на **65 KB**.
* Добавлен HTTP worker stack high-water-mark logging.

### Исправления

* Исправлен `EVENT_TYPE_COUNT`, из-за которого новые события могли теряться при превышении старого hardcoded count.

### Тестирование

v0.0.15 протестирован на Sony Ericsson J108:

* BT PAN connect
* BT PAN reconnect
* Wi-Fi failover
* независимое состояние BT/Wi-Fi для NAT
* proxy relay
* внешний сайт
* HTML
* CSS
* изображения

Релиз:

**v0.0.15**

Firmware:

```text
firmware/satura-bridge-v0.0.15.bin
```

---

## Быстрый старт

### 1. Прошивка

См. [`FLASH.md`](FLASH.md).

Или прошейте:

```text
firmware/satura-bridge-v0.0.15.bin
```

### 2. Bluetooth

1. Включите Bluetooth.
2. Найдите **Satura Bridge**.
3. Выполните pairing.
4. PIN: `0000`, если запрашивается.

### 3. Настройка Wi-Fi

Откройте:

```text
http://192.168.7.1
```

и настройте Wi-Fi.

---

## Веб-интерфейс

| Страница | Назначение |
| -------- | ---------- |
| `/` | Статус, RSSI, uptime, heap |
| `/setup` | Настройка Wi-Fi |
| `/networks` | Сканирование и управление сетями |
| `/proxy` | HTTP proxy |
| `/reset` | Удаление Wi-Fi сетей |
| `/reboot` | Перезагрузка |

---

## Совместимость

| Устройство / ПО | Результат |
| ---------------- | --------- |
| Sony Ericsson J108 | Протестировано |
| Старые Android | Работает |
| NetFront 3.4 | Работает |
| J2ME / Opera Mini | Работает |
| Встроенный email | Работает |

Windows 10 пока не поддерживается.

---

## Roadmap

* [x] Bluetooth Classic PAN
* [x] Wi-Fi Internet
* [x] NAT / routing
* [x] Web configuration
* [x] Multiple Wi-Fi networks
* [x] Wi-Fi scanning
* [x] Connection recovery
* [x] Watchdog
* [x] Event bus
* [x] Storage abstraction
* [x] HTTP proxy
* [x] POST body forwarding
* [x] Proxy hostname resolution
* [x] Transport-agnostic link abstraction
* [ ] Bluetooth PPP
* [ ] Serial / IR
* [ ] Ethernet uplink
* [ ] Cellular modem uplink
* [ ] Больше тестов Nokia Symbian
* [ ] Больше тестов Sony Ericsson
* [ ] Улучшение совместимости
* [ ] Улучшение Bluetooth / Wi-Fi coexistence
* [ ] Компактная собственная PCB

---

## Сборка

Требуется ESP-IDF v5.4.x.

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

Для чистой пересборки:

```bash
idf.py fullclean
idf.py build
```

---

## Железо

| Компонент | Рекомендуемый | Минимальный |
| --------- | ------------- | ----------- |
| Плата | M5Stack Core2 | ESP32-WROOM-32 |
| Питание | Встроенная батарея | USB 5V |

Текущее тестовое устройство:

```text
ESP32-D0WDQ6-V3
```

---

# Похожие проекты

## [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge предоставляет интернет соединение для старых S60 v1 девайсов, по Bluetooth PPP, включая Nokia N-Gage. В общем, те аппараты, в которых еще не было поддержки Bluetooth PAN.

На стороне телефона используется GnuBox для соединения.

Satura Bridge и Vetera Bridge нацелены на разные поколения устройств с разным типом подключения.

Создан и поддерживается [@arifwn](https://github.com/arifwn) на GitHub.

---

## Лицензия

MIT License.

BTstack в `components/btstack/` распространяется по собственной лицензии:

[`components/btstack/LICENSE`](components/btstack/LICENSE)

---

## Автор и сообщество

**Автор:** [@sigdev](https://github.com/sigildeveloper)

**Telegram:** [@nnmidletschat](https://t.me/nnmidletschat)

**Vintage phones • Symbian • J2ME • Retro networking**
