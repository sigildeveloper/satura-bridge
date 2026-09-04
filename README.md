# Satura Bridge

**«Bringing old phones back online.»**

Open-source Bluetooth Classic PAN → Internet gateway for legacy mobile phones and Symbian devices, running on ESP32.

Satura Bridge currently uses **Bluetooth Classic PAN as the downlink** and **Wi-Fi as the Internet uplink**.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.15-green.svg)](https://github.com/sigildeveloper/satura-bridge/releases/tag/v0.0.15)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

# What is Satura Bridge?

Satura Bridge is a small ESP32-based Bluetooth Classic PAN gateway for legacy mobile phones.

The phone connects through Bluetooth PAN, while the ESP32 uses Wi-Fi as its Internet uplink and performs NAT between the two links.

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

> **Current transport support:** Bluetooth Classic PAN as downlink and Wi-Fi as uplink. The transport abstraction is ready for additional implementations, but Bluetooth PPP, Ethernet, Serial/IR and cellular modem transports are not currently implemented as user-facing features.

> **Compatibility:** Windows 10 Internet connectivity is currently not supported. Older Android devices, Sony Ericsson J108, NetFront 3.4 and J2ME applications have been tested successfully.

---

# Supported Hardware

Satura Bridge now has separate board profiles.

| Board                    | Flash | Battery / Power                   | Build profile |
| ------------------------ | ----- | --------------------------------- | ------------- |
| Generic ESP32 DevKit (ESP32-WROOM-32 recommended)     | 2 MB  | No board-specific battery support | `generic`     |
| M5Stack Core2 (original) | 16 MB | AXP192 battery / charging         | `core2`       |
| M5StickC Plus2           | 8 MB  | Battery voltage via ADC           | `plus2`       |

### M5Stack Core2

The supported Core2 revision is the **original M5Stack Core2 using the AXP192 PMIC**.

The Core2 v1.1 revision, which uses the AXP2101 PMIC, is not currently supported by the Core2 board backend.

### M5StickC Plus2

The Plus2 uses:

* ESP32-PICO-V3-02
* 8 MB flash
* 2 MB PSRAM
* 200 mAh battery
* GPIO4 for power hold
* GPIO38 for battery voltage measurement

The Plus2 does not currently expose a documented charging-status signal, so the firmware reports battery percentage but does not report charging state.

### Generic ESP32

The Generic profile is intended for ESP32 boards without board-specific power-management hardware.

It uses a **2 MB flash configuration**.

---

# Features

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
* Board abstraction layer
* Independent build configurations for supported boards
* Single-file full firmware images

---

# HTTP Proxy Gateway

Configure the optional HTTP proxy at:

```text
http://192.168.7.1/proxy
```

The gateway can be specified using either an IPv4 address or a hostname.

Hostnames are resolved when the proxy configuration is loaded or changed, and the resulting IPv4 address is cached.

The proxy relay supports HTTP requests including POST bodies.

For compatibility with older browsers and WAP software, proxy responses use HTTP/1.0-style framing with `Content-Length` or connection-close semantics instead of HTTP/1.1 chunked encoding.

### Example gateway: 15pmm01.com

[15pmm01.com](https://15pmm01.com/wap/en/) is a public WAP compression gateway that works well as a proxy target for old browsers. It re-compresses and simplifies pages on the fly, which is useful for the small screens and slow connections this project targets.

It is built and maintained by [@petarmarinov37](https://t.me/petarmarinov37) on Telegram.

---

# Architecture

The networking core is transport-agnostic.

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

## `link_iface`

`link_iface_t` provides a generic interface for a network link.

The networking core does not need to know whether the underlying link is Bluetooth, Wi-Fi, Ethernet, a cellular modem or another transport.

## `link_registry`

`link_registry` keeps track of the currently active link for each role:

* `DOWNLINK`
* `UPLINK`

## `nat_bridge`

`nat_bridge` performs the bridge/NAT functionality through the generic link interface instead of depending directly on Bluetooth PAN or Wi-Fi.

This allows new transports to be added without rewriting the NAT, HTTP or DNS layers.

## Event bus

Generic link-state events are available:

* `EVENT_DOWNLINK_UP`
* `EVENT_DOWNLINK_DOWN`
* `EVENT_UPLINK_UP`
* `EVENT_UPLINK_DOWN`

Transport-specific modules can publish these events alongside their own transport-specific events.

## Future transports

The architecture is intended to allow implementations such as:

* Bluetooth PPP
* Serial / IR
* Ethernet
* Cellular / 4G / 5G modem
* Other network interfaces

These are not currently implemented as user-facing transports.

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

* Wi-Fi manager uses a single-owner queue-based state machine.
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
* HTTP proxy relay traffic
* HTML, CSS and image loading through the proxy

---

# Quick Start

## 1. Build

Requires **ESP-IDF v5.4.x**.

Clone the repository:

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
```

### Build all supported boards

On Windows PowerShell:

```powershell
.\build.ps1
```

This builds:

```text
Generic ESP32 DevKit
M5Stack Core2
M5StickC Plus2
```

and creates independent build directories:

```text
build/
├── generic/
├── core2/
└── stickc_plus2/
```

The generated full firmware images are placed in:

```text
firmware/
├── generic/
│   └── satura-bridge-generic-full.bin
├── core2/
│   └── satura-bridge-core2-full.bin
└── stickc_plus2/
    └── satura-bridge-stickc_plus2-full.bin
```

The build script does not flash the device.

### Build a specific board

```powershell
.\build.ps1 generic
.\build.ps1 core2
.\build.ps1 plus2
```

### Rebuild

Build everything from a clean build directory:

```powershell
.\build.ps1 rebuild
```

Or rebuild a specific board:

```powershell
.\build.ps1 rebuild core2
```

### Clean build directories

```powershell
.\build.ps1 clean
```

---

# ESP-IDF Configuration

Each board has its own independent ESP-IDF `sdkconfig`.

```text
build/
├── generic/
│   └── sdkconfig
├── core2/
│   └── sdkconfig
└── stickc_plus2/
    └── sdkconfig
```

The board-specific defaults are:

```text
sdkconfig.defaults
sdkconfig.defaults.generic
sdkconfig.defaults.core2
sdkconfig.defaults.stickc_plus2
```

The common configuration is stored in:

```text
sdkconfig.defaults
```

Board-specific settings are stored in the corresponding board defaults file.

## Menuconfig

Open menuconfig for Generic ESP32:

```powershell
idf.py -B build\generic menuconfig
```

For M5Stack Core2:

```powershell
idf.py -B build\core2 menuconfig
```

For M5StickC Plus2:

```powershell
idf.py -B build\stickc_plus2 menuconfig
```

Changes made through menuconfig are stored in that board's build directory and do not modify the other board configurations.

---

# Flashing

See [FLASH.md](FLASH.md) for detailed flashing instructions.

For example, to flash an M5Stack Core2 using ESP-IDF:

```powershell
idf.py -B build\core2 -p COM6 flash
```

To open the serial monitor:

```powershell
idf.py -B build\core2 -p COM6 monitor
```

Or both:

```powershell
idf.py -B build\core2 -p COM6 flash monitor
```

---

# Web Interface

The web interface is available at:

```text
http://192.168.7.1
```

| Page        | Description                               |
| ----------- | ----------------------------------------- |
| `/`         | Status, RSSI, uptime and heap information |
| `/setup`    | Manual Wi-Fi configuration                |
| `/networks` | Scan and manage saved Wi-Fi networks      |
| `/proxy`    | Configure HTTP proxy gateway              |
| `/reset`    | Remove all saved Wi-Fi networks           |
| `/reboot`   | Restart the device                        |

---

# First Boot

After flashing the firmware:

1. Enable Bluetooth on the phone.
2. Find **Satura Bridge**.
3. Pair and connect the phone.
4. If a PIN is requested, use `0000`.
5. Open the phone browser.
6. Open `http://192.168.7.1` or any HTTP URL.
7. Configure Wi-Fi.

Saved Wi-Fi settings survive reboot and the device automatically attempts to reconnect.

---

# Performance

Typical measured values:

| Parameter                     | Value             |
| ----------------------------- | ----------------- |
| Download                      | ~0.15–0.20 Mbit/s |
| Upload                        | ~0.20–0.27 Mbit/s |
| Ping                          | 150–200 ms        |
| Maximum Bluetooth PAN clients | 1                 |
| Power draw                    | ~200 mA from USB  |

Actual performance depends on RF conditions and the Bluetooth PAN implementation.

The available performance is normally sufficient for:

* retro browsers
* lightweight web pages
* messaging
* J2ME applications
* WAP-related software

---

# Compatibility

| Device / Platform     | Result  |
| --------------------- | ------- |
| Sony Ericsson J108    | Tested  |
| Older Android devices | Working |
| NetFront 3.4          | Working |
| J2ME / Opera Mini     | Working |
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
* [x] Board abstraction layer
* [x] Generic ESP32 board profile
* [x] M5Stack Core2 board profile
* [x] M5StickC Plus2 board profile
* [x] Independent board build configurations
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

# Project Structure

```text
satura-bridge/
├── main/
│   ├── board/
│   │   ├── board.h
│   │   ├── board_generic.c
│   │   ├── board_core2.c
│   │   └── board_stickc_plus2.c
│   │
│   ├── main.c
│   ├── app_bootstrap.c
│   ├── app_state.c
│   ├── battery.c
│   ├── bt_pan.c
│   ├── clipboard.c
│   ├── config.h
│   ├── device_name.c
│   ├── dns_server.c
│   ├── event_bus.c
│   ├── http_routes.c
│   ├── http_server.c
│   ├── http_utils.c
│   ├── link_iface.h
│   ├── link_registry.c
│   ├── nat_bridge.c
│   ├── nvs_storage.c
│   ├── proxy_gateway.c
│   ├── proxy_relay.c
│   ├── storage.c
│   ├── uptime.c
│   ├── watchdog.c
│   ├── wifi_manager.c
│   ├── Kconfig
│   └── CMakeLists.txt
│
├── components/
│   └── ...
│
├── firmware/
│   ├── generic/
│   ├── core2/
│   └── stickc_plus2/
│
├── sdkconfig.defaults
├── sdkconfig.defaults.generic
├── sdkconfig.defaults.core2
├── sdkconfig.defaults.stickc_plus2
├── build.ps1
├── CMakeLists.txt
├── FLASH.md
├── CHANGELOG.md
└── README.md
```

Build output under `build/` is generated locally and is not part of the source tree.

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

```text
components/btstack/LICENSE
```

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

# Что такое Satura Bridge?

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

# Поддерживаемое железо

| Плата                                | Flash | Профиль сборки |
| ------------------------------------ | ----- | -------------- |
| Generic ESP32 DevKit (рекомендуется ESP32-WROOM-32) | 2 MB  | `generic`      |
| M5Stack Core2 (оригинальный, AXP192) | 16 MB | `core2`        |
| M5StickC Plus2                       | 8 MB  | `plus2`        |

Для Core2 поддерживается оригинальная ревизия с **AXP192**.

Core2 v1.1 с **AXP2101** пока не поддерживается.

M5StickC Plus2 использует GPIO4 для удержания питания после запуска и GPIO38 для измерения напряжения аккумулятора.

---

# Сборка

Требуется **ESP-IDF v5.4.x**.

После клонирования репозитория:

```powershell
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
```

## Собрать все платы

```powershell
.\build.ps1
```

Будут собраны:

```text
Generic ESP32 DevKit
M5Stack Core2
M5StickC Plus2
```

Для каждой платы используется отдельный каталог:

```text
build/
├── generic/
├── core2/
└── stickc_plus2/
```

Готовые полные прошивки:

```text
firmware/
├── generic/
│   └── satura-bridge-generic-full.bin
├── core2/
│   └── satura-bridge-core2-full.bin
└── stickc_plus2/
    └── satura-bridge-stickc_plus2-full.bin
```

Скрипт сборки **не прошивает устройство**.

## Собрать одну плату

```powershell
.\build.ps1 generic
.\build.ps1 core2
.\build.ps1 plus2
```

## Полная пересборка

```powershell
.\build.ps1 rebuild
```

Или:

```powershell
.\build.ps1 rebuild core2
```

## Menuconfig

Для Generic:

```powershell
idf.py -B build\generic menuconfig
```

Для Core2:

```powershell
idf.py -B build\core2 menuconfig
```

Для Plus2:

```powershell
idf.py -B build\stickc_plus2 menuconfig
```

Каждая плата имеет собственный `sdkconfig`.

Общие настройки находятся в:

```text
sdkconfig.defaults
```

Настройки плат:

```text
sdkconfig.defaults.generic
sdkconfig.defaults.core2
sdkconfig.defaults.stickc_plus2
```

---

# Прошивка

Для подробной инструкции см. [`FLASH.md`](FLASH.md).

Например, для M5Stack Core2 на COM6:

```powershell
idf.py -B build\core2 -p COM6 flash
```

Прошить и открыть монитор:

```powershell
idf.py -B build\core2 -p COM6 flash monitor
```

---

# Архитектура v0.0.15

Сетевое ядро не зависит от конкретного транспорта.

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

# Возможности

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
* Board abstraction layer
* Независимые конфигурации для разных плат

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

# Первый запуск

После прошивки:

1. Включите Bluetooth.
2. Найдите **Satura Bridge**.
3. Выполните pairing.
4. PIN: `0000`, если запрашивается.
5. Откройте браузер телефона.
6. Откройте:

```text
http://192.168.7.1
```

7. Настройте Wi-Fi.

Сохранённые Wi-Fi сети переживают перезагрузку.

---

# Веб-интерфейс

| Страница    | Назначение                       |
| ----------- | -------------------------------- |
| `/`         | Статус, RSSI, uptime, heap       |
| `/setup`    | Настройка Wi-Fi                  |
| `/networks` | Сканирование и управление сетями |
| `/proxy`    | HTTP proxy                       |
| `/reset`    | Удаление Wi-Fi сетей             |
| `/reboot`   | Перезагрузка                     |

---

# Совместимость

| Устройство / ПО    | Результат      |
| ------------------ | -------------- |
| Sony Ericsson J108 | Протестировано |
| Старые Android     | Работает       |
| NetFront 3.4       | Работает       |
| J2ME / Opera Mini  | Работает       |
| Встроенный email   | Работает       |

Windows 10 пока не поддерживается.

---

# Roadmap

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
* [x] Board abstraction layer
* [x] Generic ESP32 profile
* [x] M5Stack Core2 profile
* [x] M5StickC Plus2 profile
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

# Похожие проекты

## [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge предоставляет интернет-соединение для старых S60 v1 устройств по Bluetooth PPP, включая Nokia N-Gage.

На стороне телефона используется GnuBox.

Satura Bridge и Vetera Bridge нацелены на разные поколения устройств и используют разные сетевые технологии.

Создан и поддерживается [@arifwn](https://github.com/arifwn).

---

# Лицензия

MIT License.

BTstack в `components/btstack/` распространяется по собственной лицензии.

```text
components/btstack/LICENSE
```

---

# Автор и сообщество

**Автор:** [@sigdev](https://github.com/sigildeveloper)

**Telegram:** [@nnmidletschat](https://t.me/nnmidletschat)

**Vintage phones • Symbian • J2ME • Retro networking**
