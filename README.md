# Satura Bridge

**«Bringing old phones back online.»**

Open-source Bluetooth Classic PAN → Internet gateway for legacy mobile phones and Symbian devices, running on ESP32.

Satura Bridge currently uses **Bluetooth Classic PAN as the downlink** and **Wi-Fi as the Internet uplink**.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.16-green.svg)](https://github.com/sigildeveloper/satura-bridge/releases/tag/v0.0.16)
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
| Generic ESP32 DevKit (ESP32-WROOM-32U recommended) | 2 MB  | No board-specific battery support | `generic`     |
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
* Hidden Wi-Fi network support
* Wi-Fi scanning and signal-based network selection
* Automatic connection recovery and failover
* Watchdog and component recovery
* Shared web clipboard / pasteboard
* Persistent clipboard history and favorites
* Automatic HTTP/HTTPS link detection in clipboard
* Configurable Bluetooth device name
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

# Wi-Fi Network Management

Satura Bridge can store up to **6 Wi-Fi networks** and automatically select a suitable saved network.

Open:

```text
http://192.168.7.1/networks
```

The page provides:

* Wi-Fi scanning
* nearby access-point list with RSSI
* saved-network management
* adding and removing networks
* automatic network selection
* manual connection attempts
* support for hidden SSIDs

### Hidden Wi-Fi networks

A hidden network does not broadcast its SSID in beacon frames, so it normally cannot appear in a scan result.

To add one:

1. Open `/networks`.
2. Use **Add Network**.
3. Enter the exact SSID.
4. Enter the password, if required.
5. Enable **This network is hidden (not broadcasting its name)**.
6. Save the network.

Satura Bridge will then attempt the saved hidden network even when it is absent from the scan results.

The hidden flag is stored together with the saved network and survives reboot.

> **Upgrade note:** v0.0.16 changes the stored Wi-Fi network structure by adding the hidden-network flag. Existing saved Wi-Fi credentials from older firmware may need to be added again after upgrading.

---

# Clipboard / Pasteboard

Satura Bridge includes a small shared web clipboard designed specifically for old phones with awkward text input.

Open:

```text
http://192.168.7.1/clip
```

The same clipboard can be used from:

* the legacy phone connected through Bluetooth PAN;
* a computer connected to the Wi-Fi network/LAN;
* another device that can reach the bridge web interface.

### Typical use

For example, typing a long URL on a T9 keypad is inconvenient.

Instead:

1. Connect the old phone to Satura Bridge over Bluetooth PAN.
2. From a computer, open `http://192.168.7.1/clip`.
3. Paste or type the long URL/text into the clipboard.
4. Save it.
5. Open the clipboard page from the phone.
6. Use the saved entry on the phone.

This is also useful for:

* long URLs
* long text snippets
* WEP keys
* game unlock codes
* configuration strings
* other text that is painful to enter on a legacy keypad

### Clipboard storage

The clipboard has:

* **15 recent entries** in a persistent ring buffer;
* up to **8 pinned/favorite entries**;
* persistent storage across reboots;
* delete and pin/unpin actions;
* automatic recognition of HTTP/HTTPS links.

When the recent-entry limit is reached, the oldest non-favorite entries are removed first. Pinned favorites are kept separately.

---

# Bluetooth Device Name

The Bluetooth device name can be configured at:

```text
http://192.168.7.1/name
```

This is the name displayed by the phone when scanning for or pairing with Satura Bridge.

On the first boot, if no custom name has been saved, Satura Bridge generates a unique default name such as:

```text
Satura Bridge A1B2
```

The suffix is derived from the last two bytes of the Bluetooth MAC address, so several bridges nearby do not all appear with the same name.

To change it:

1. Open `/name`.
2. Enter the desired name.
3. Save it.
4. Reboot the bridge.

The name is stored persistently in NVS and is used by the Bluetooth stack on the next boot.

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

# v0.0.16

v0.0.16 is a **feature and multi-board release** focused on making Satura Bridge easier to use with real legacy phones while expanding hardware support.

## Wi-Fi

* Added support for **hidden Wi-Fi networks**.
* Hidden networks can be marked directly from `/setup` and `/networks`.
* Hidden saved networks are attempted even when they are not present in scan results.
* Wi-Fi disconnect logs now include the actual reason code, making real-world connection failures easier to diagnose.
* Improved network management and scan-result presentation.

## Clipboard

* Added a shared web clipboard / pasteboard at `/clip`.
* Clipboard data can be entered from a computer and read from the legacy phone.
* Added persistent recent-entry history.
* Added pinned/favorite entries.
* Added HTTP/HTTPS link detection.
* Fixed clipboard entries so text can be selected and copied by older browsers.
* Fixed URL decoding so pasted links are stored as normal URLs instead of percent-encoded text.

## Bluetooth device name

* Added `/name` for changing the Bluetooth device name.
* Custom names persist across reboots.
* The default name is generated as `Satura Bridge XXXX` from the Bluetooth MAC address.
* The new Bluetooth name is applied after reboot.

## M5Stack Core2 battery status

* Added battery information for the original M5Stack Core2 using the AXP192 PMIC.
* The status page can show battery percentage and charging state when supported by the selected board.

## Web interface

* Added dedicated **Manage Networks**, **Device Name** and **Clipboard** pages.
* Improved the networks page by placing scan results near the top and providing a nearby “scan again” action.
* Status page now exposes additional runtime information including battery status and proxy state.
* Proxy configuration validates the port value server-side.

## Bluetooth reliability

* Improved recovery when the HCI connection disappears before BNEP is fully opened.
* The bridge automatically restores Bluetooth visibility after disconnection, with delayed reopening to avoid busy loops.

## Stability and security

* Improved handling of NVS initialization failures on first boot.
* Fixed a possible buffer overflow while loading saved networks.
* Fixed DNS upstream source validation.
* Improved proxy timeout handling.
* Increased HTTP server stack size and added stack-headroom monitoring.
* Legacy PIN pairing responds with `0000`.
* SSP user confirmation is automatically accepted.

## Multi-board support

* Added a board abstraction layer under `main/board/`.
* Added separate profiles for:
  * Generic ESP32 DevKit
  * M5Stack Core2 (original, AXP192)
  * M5StickC Plus2
* Added independent ESP-IDF configurations and build directories.
* Added `build.ps1` for reproducible board-specific builds.
* Added automatic generation of complete firmware images for each board.

## Firmware

The release contains separate full firmware images:

```text
firmware/
├── generic/
│   └── satura-bridge-generic-full.bin
├── core2/
│   └── satura-bridge-core2-full.bin
└── stickc_plus2/
    └── satura-bridge-stickc_plus2-full.bin
```

Each full image contains the bootloader, partition table and application and is intended to be flashed starting at `0x0`.

## Migration note

The saved Wi-Fi network structure changed in v0.0.16 because each network now stores a hidden-network flag. After upgrading from an older firmware version, re-add saved Wi-Fi networks if they are no longer recognized.


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

| Page        | Description                                           |
| ----------- | ----------------------------------------------------- |
| `/`         | Status, RSSI, uptime, heap, battery and proxy state   |
| `/setup`    | Manual Wi-Fi configuration                            |
| `/networks` | Scan and manage saved Wi-Fi networks, including hidden SSIDs |
| `/proxy`    | Configure HTTP proxy gateway                          |
| `/clip`     | Shared clipboard / pasteboard                         |
| `/name`     | Configure the Bluetooth device name                   |
| `/reset`    | Remove all saved Wi-Fi networks                       |
| `/reboot`   | Restart the device                                    |

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
* [x] Hidden Wi-Fi networks
* [x] Wi-Fi scanning
* [x] Connection recovery
* [x] Watchdog
* [x] Event bus
* [x] Storage abstraction
* [x] HTTP proxy gateway
* [x] Web clipboard / pasteboard
* [x] Clipboard history and favorites
* [x] Configurable Bluetooth device name
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
| Generic ESP32 DevKit (рекомендуется ESP32-WROOM-32U) | 2 MB  | `generic`      |
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
* Поддержка скрытых Wi-Fi сетей
* Сканирование Wi-Fi
* Автоматический выбор сети
* Recovery / failover
* Watchdog
* Веб-клипборд / буфер обмена
* История и избранные записи клипборда
* Переименование Bluetooth-устройства
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

## Управление Wi-Fi сетями

Satura Bridge может хранить до **6 Wi-Fi сетей** и автоматически выбирать подходящую сохранённую сеть.

Откройте:

```text
http://192.168.7.1/networks
```

Здесь доступны:

* сканирование Wi-Fi;
* список найденных точек с RSSI;
* управление сохранёнными сетями;
* добавление и удаление сетей;
* автоматический выбор сети;
* ручная попытка подключения;
* поддержка скрытых SSID.

### Скрытые Wi-Fi сети

Скрытая сеть не передаёт своё имя в beacon-пакетах, поэтому обычно не появляется в результатах сканирования.

Чтобы добавить такую сеть:

1. Откройте `/networks`.
2. Выберите **Add Network**.
3. Введите точный SSID.
4. Введите пароль, если он нужен.
5. Включите **This network is hidden (not broadcasting its name)**.
6. Сохраните сеть.

После этого Satura Bridge будет пытаться подключиться к сохранённой скрытой сети, даже если её нет среди результатов сканирования.

Признак скрытой сети сохраняется вместе с сетью и переживает перезагрузку.

> **Важно при обновлении:** в v0.0.16 изменилась структура сохранённых Wi-Fi сетей — добавлено поле hidden. После обновления со старой прошивки сохранённые сети может потребоваться добавить заново.

---

# Клипборд / буфер обмена

В Satura Bridge есть общий веб-клипборд, специально предназначенный для старых телефонов, на которых неудобно вводить длинный текст.

Откройте:

```text
http://192.168.7.1/clip
```

Один и тот же клипборд доступен:

* со старого телефона через Bluetooth PAN;
* с компьютера в Wi-Fi/LAN;
* с другого устройства, имеющего доступ к веб-интерфейсу моста.

### Как пользоваться

Например, длинный URL неудобно вводить на T9-клавиатуре.

1. Подключите старый телефон к Satura Bridge по Bluetooth PAN.
2. На компьютере откройте `http://192.168.7.1/clip`.
3. Вставьте или напечатайте длинный URL/текст.
4. Сохраните запись.
5. Откройте `/clip` с телефона.
6. Используйте сохранённую запись на телефоне.

Клипборд особенно полезен для:

* длинных URL;
* длинного текста;
* WEP-ключей;
* кодов разблокировки игр;
* строк конфигурации;
* других данных, которые неудобно набирать на старом телефоне.

### Хранение

Клипборд имеет:

* **15 последних записей**;
* до **8 закреплённых / избранных записей**;
* сохранение после перезагрузки;
* удаление и закрепление/открепление записей;
* автоматическое распознавание HTTP/HTTPS ссылок.

Когда история заполняется, самые старые обычные записи вытесняются. Закреплённые записи хранятся отдельно.

---

# Имя Bluetooth-устройства

Имя Bluetooth можно изменить по адресу:

```text
http://192.168.7.1/name
```

Это имя видит телефон при поиске и сопряжении с Satura Bridge.

Если пользовательское имя ещё не задано, при первом запуске создаётся уникальное имя вида:

```text
Satura Bridge A1B2
```

`A1B2` формируется из последних двух байт Bluetooth MAC. Поэтому несколько мостов рядом не будут отображаться как одинаковые устройства.

Чтобы изменить имя:

1. Откройте `/name`.
2. Введите новое имя.
3. Сохраните.
4. Перезагрузите мост.

Имя сохраняется в NVS и применяется Bluetooth-стеком при следующем запуске.

---

# HTTP Proxy

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

| Страница    | Назначение                                      |
| ----------- | ---------------------------------------------- |
| `/`         | Статус, RSSI, uptime, heap, батарея, proxy      |
| `/setup`    | Настройка Wi-Fi                                |
| `/networks` | Сканирование и управление сетями, включая hidden SSID |
| `/proxy`    | Настройка HTTP proxy                            |
| `/clip`     | Общий клипборд / буфер обмена                   |
| `/name`     | Переименование Bluetooth-устройства             |
| `/reset`    | Удаление сохранённых Wi-Fi сетей                |
| `/reboot`   | Перезагрузка                                    |

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
* [x] Hidden Wi-Fi networks
* [x] Wi-Fi scanning
* [x] Connection recovery
* [x] Watchdog
* [x] Event bus
* [x] Storage abstraction
* [x] HTTP proxy
* [x] Web clipboard / pasteboard
* [x] Clipboard history and favorites
* [x] Configurable Bluetooth device name
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

MIT License. Указание авторства, остальное не важно.

BTstack в `components/btstack/` распространяется по собственной лицензии.

```text
components/btstack/LICENSE
```

---

# Автор и сообщество

**Автор:** [@sigdev](https://github.com/sigildeveloper)

**Telegram:** [@nnmidletschat](https://t.me/nnmidletschat)

**Vintage phones • Symbian • J2ME • Retro networking**
