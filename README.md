# Satura Bridge

**Bringing old phones back online.**

Satura Bridge is an open-source **Bluetooth Classic PAN → Wi-Fi Internet gateway** for legacy mobile phones, Symbian devices and other hardware that can use Bluetooth PAN but lacks a practical modern Internet connection.

It runs on an **ESP32**. The phone connects to the ESP32 over Bluetooth PAN, while the ESP32 connects to an existing Wi-Fi network and performs routing/NAT between the two networks.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.16-green.svg)](https://github.com/sigildeveloper/satura-bridge/releases/tag/v0.0.16)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

> **Current transport:** Bluetooth Classic PAN/NAP downlink + Wi-Fi uplink.
>
> **Not implemented:** Bluetooth PPP, Ethernet, serial/IR and cellular modem transports.

---

## Why?

A surprising number of old phones still have usable browsers, mail clients, J2ME applications and Symbian software, but their original network infrastructure has disappeared or become inconvenient.

Satura Bridge provides a small local gateway:

```text
┌────────────────────┐
│    Legacy phone    │
│ Nokia / Sony       │
│ Ericsson / Symbian │
└─────────┬──────────┘
          │ Bluetooth Classic / PAN
          ▼
┌────────────────────┐
│   Satura Bridge    │
│       ESP32        │
│    PAN ↔ NAT ↔ Wi-Fi
└─────────┬──────────┘
          │ Wi-Fi
          ▼
┌────────────────────┐
│  Existing router   │
└─────────┬──────────┘
          ▼
       Internet
```

The phone does not need a Satura-specific application. It only needs a compatible Bluetooth PAN implementation.

---

## Current status

**v0.0.16 — development release.**

The core gateway is functional and has been tested with real legacy devices. Compatibility with a particular phone should still be treated as something to test rather than assumed from its age or manufacturer.

### Tested / working

| Device / software | Status |
| --- | --- |
| Sony Ericsson J108 | **Tested** |
| Older Android devices | **Working** |
| NetFront 3.4 | **Working** |
| J2ME applications | **Working** |
| Opera Mini | **Working** |
| Built-in email client | **Working** |

### Known limitations

- Bluetooth Classic PAN is currently the only implemented phone-side transport.
- Maximum tested PAN client count: **1**.
- Windows 10 Internet connectivity is **not currently supported** as a Satura Bridge PAN client.
- Bluetooth PPP is not implemented.
- Some legacy phones require device-specific PAN configuration.
- Throughput depends strongly on the phone's Bluetooth PAN implementation and RF conditions.

If you test another phone, please report the exact model and firmware/OS version.

---

# Hardware

| Board | Flash | Build profile | Notes |
| --- | ---: | --- | --- |
| Generic ESP32 DevKit | 2 MB | `generic` | ESP32-WROOM-32/32U class boards; WROOM-32U recommended |
| M5Stack Core2 (original) | 16 MB | `core2` | AXP192 PMIC |
| M5StickC Plus2 | 8 MB | `plus2` | ESP32-PICO-V3-02, 2 MB PSRAM |

### M5Stack Core2

Only the **original Core2 revision with AXP192** is supported by the Core2 board backend.

**Core2 v1.1 with AXP2101 is not currently supported.**

### M5StickC Plus2

The Plus2 backend supports 8 MB flash, 2 MB PSRAM, a 200 mAh battery, GPIO4 power hold and GPIO38 battery-voltage measurement. Battery level is supported; charging-state reporting is not currently exposed.

### Generic ESP32

The Generic profile is intended for ESP32 boards without board-specific power-management hardware. The current profile is configured for **2 MB flash**.

---

# Features

### Networking

- Bluetooth Classic PAN / NAP
- Wi-Fi Internet uplink
- NAT / routing between PAN and Wi-Fi
- Transport-independent network-link abstraction
- Separate `DOWNLINK` and `UPLINK` roles
- Generic link-state event bus
- Wi-Fi recovery and failover
- Watchdog and component recovery
- DNS forwarding with upstream source validation

### Wi-Fi management

- Up to **6 saved Wi-Fi networks**
- Wi-Fi scanning and RSSI display
- Automatic selection of saved networks
- Manual connection attempts
- Hidden SSID support
- Persistent configuration

### Web interface

- Status page
- Wi-Fi configuration and network management
- HTTP proxy configuration
- Shared clipboard / pasteboard
- Bluetooth device-name configuration
- Reboot and network-reset controls
- Runtime information including RSSI, heap, uptime and board-specific battery data

### Legacy-browser support

- HTTP proxy relay
- HTTP POST body forwarding
- Proxy hostname resolution with cached IPv4 address
- HTTP/1.0-style response framing for older clients
- Clipboard pages designed for old browsers

### Firmware / architecture

- Board abstraction layer
- Independent ESP-IDF configurations per board
- Reproducible PowerShell build script
- Full merged firmware images
- Explicit `-Os` optimization
- HTTP worker stack monitoring

---

# Quick start

## 1. Get the firmware

For a normal installation, use the firmware attached to the corresponding [GitHub release](https://github.com/sigildeveloper/satura-bridge/releases).

Each board has its own full image:

```text
firmware/
├── generic/
│   └── satura-bridge-generic-full.bin
├── core2/
│   └── satura-bridge-core2-full.bin
└── stickc_plus2/
    └── satura-bridge-stickc_plus2-full.bin
```

A **full image** contains the bootloader, partition table and application and is intended to be written starting at **`0x0`**.

See [FLASH.md](FLASH.md) before flashing.

## 2. Flash the device

Example for M5Stack Core2 on COM6:

```powershell
idf.py -B build\core2 -p COM6 flash
```

Or flash and open the serial monitor:

```powershell
idf.py -B build\core2 -p COM6 flash monitor
```

If you are using a pre-built full image rather than `idf.py`, follow [FLASH.md](FLASH.md).

## 3. Connect the phone

1. Turn on Bluetooth on the phone.
2. Search for **Satura Bridge**.
3. Pair with the bridge.
4. If a legacy PIN is requested, use **`0000`**.
5. Establish the Bluetooth PAN connection using the phone's networking settings.
6. Open the phone browser.
7. Open **`http://192.168.7.1`**.
8. Configure the Wi-Fi uplink.

Once Wi-Fi is connected, traffic from the phone is routed through the bridge to the Internet.

> The exact PAN procedure differs between Nokia, Sony Ericsson, Symbian and Android devices. Pairing alone does not guarantee that the phone can establish a PAN connection.

---

# Building from source

## Requirements

- ESP-IDF **5.4.x**
- ESP-IDF Python environment
- `idf.py`
- `esptool`
- PowerShell on Windows for `build.ps1`

Clone the repository including submodules:

```powershell
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
```

If it was cloned without submodules:

```powershell
git submodule update --init --recursive
```

### Build all boards

```powershell
.\build.ps1
```

Build directories:

```text
build/
├── generic/
├── core2/
└── stickc_plus2/
```

Full images are generated in:

```text
firmware/
├── generic/satura-bridge-generic-full.bin
├── core2/satura-bridge-core2-full.bin
└── stickc_plus2/satura-bridge-stickc_plus2-full.bin
```

The build script **does not flash the device**.

### Build one board

```powershell
.\build.ps1 generic
.\build.ps1 core2
.\build.ps1 plus2
```

Explicit form:

```powershell
.\build.ps1 build core2
```

### Rebuild / clean

```powershell
.\build.ps1 rebuild
.\build.ps1 rebuild core2
.\build.ps1 clean
.\build.ps1 clean core2
```

### Menuconfig

```powershell
.\build.ps1 menuconfig generic
.\build.ps1 menuconfig core2
.\build.ps1 menuconfig plus2
```

Each board has an independent `sdkconfig` and board-specific defaults file.

Equivalent direct ESP-IDF commands are:

```powershell
idf.py -B build\generic menuconfig
idf.py -B build\core2 menuconfig
idf.py -B build\stickc_plus2 menuconfig
```

---

# Web interface

The Bluetooth PAN side uses the bridge's local gateway address:

```text
http://192.168.7.1
```

| Path | Purpose |
| --- | --- |
| `/` | Status, RSSI, uptime, heap, battery and proxy state |
| `/setup` | Manual Wi-Fi configuration |
| `/networks` | Scan and manage saved Wi-Fi networks |
| `/proxy` | Configure HTTP proxy gateway |
| `/clip` | Shared clipboard / pasteboard |
| `/name` | Configure Bluetooth device name |
| `/reset` | Remove saved Wi-Fi networks |
| `/reboot` | Restart the device |

The Wi-Fi/LAN address of the bridge is assigned by the upstream router and can be different from `192.168.7.1`.

---

# Wi-Fi configuration

Satura Bridge can store up to **6 networks**.

Open:

```text
http://192.168.7.1/networks
```

The page provides scanning, RSSI information, saved-network management, manual connection attempts and hidden SSID configuration.

### Hidden SSIDs

1. Open `/networks`.
2. Select **Add Network**.
3. Enter the exact SSID.
4. Enter the password if required.
5. Enable **This network is hidden (not broadcasting its name)**.
6. Save the network.

The hidden flag is persistent, so the bridge can attempt the network even when it is absent from scan results.

### v0.0.16 migration note

v0.0.16 changed the stored Wi-Fi network structure by adding the hidden-network flag. After upgrading from an older firmware version, previously saved networks may need to be added again.

---

# Shared clipboard

The web clipboard is designed for old phones where entering long text on a T9 keypad is inconvenient.

Open:

```text
http://192.168.7.1/clip
```

The same clipboard can be accessed from the legacy phone over Bluetooth PAN and from a computer or other device on the Wi-Fi/LAN side.

### Example: computer → phone

1. Connect the phone to Satura Bridge.
2. From a computer, open the bridge's Wi-Fi/LAN IP and add `/clip`.
3. Paste the required URL or text.
4. Save it.
5. Open `http://192.168.7.1/clip` from the phone.
6. Use the saved entry.

Storage:

- **15 recent entries** in a persistent ring buffer;
- up to **8 pinned/favorite entries**;
- persistence across reboots;
- delete and pin/unpin actions;
- automatic HTTP/HTTPS link detection.

---

# Bluetooth device name

Configure the Bluetooth name at:

```text
http://192.168.7.1/name
```

If no custom name is configured, the bridge generates a unique name such as:

```text
Satura Bridge A1B2
```

The suffix is derived from the last two bytes of the Bluetooth MAC address.

After changing the name, reboot the bridge so the Bluetooth stack applies it on startup.

---

# HTTP proxy gateway

Configure the optional proxy at:

```text
http://192.168.7.1/proxy
```

The gateway can be an IPv4 address or a hostname. Hostnames are resolved and the resulting IPv4 address is cached.

The proxy supports HTTP requests including POST bodies. For old browsers and WAP software, responses use HTTP/1.0-style framing with `Content-Length` or connection-close semantics instead of relying on HTTP/1.1 chunked encoding.

### Example: 15pmm01.com

[15pmm01.com](https://15pmm01.com/wap/en/) is a public WAP-oriented compression gateway that can be useful with old browsers and slow connections. It is maintained by [@petarmarinov37](https://t.me/petarmarinov37).

> The proxy is optional. Satura Bridge does not require a WAP proxy for basic IP connectivity.

---

# Architecture

The networking core is intentionally independent of the physical transport:

```text
                    ┌──────────────────────┐
                    │      HTTP / DNS       │
                    │     configuration     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │      nat_bridge       │
                    │  transport-independent│
                    └──────────┬───────────┘
                               │
                    link_registry / link_iface
                         │                │
                      DOWNLINK          UPLINK
                         │                │
                    ┌────▼────┐      ┌────▼────┐
                    │ bt_pan  │      │  Wi-Fi  │
                    └─────────┘      └─────────┘
```

### `link_iface_t`

Generic network-link interface. The networking core does not need to know whether the link is Bluetooth, Wi-Fi, Ethernet or another transport.

### `link_registry`

Tracks the active link for `DOWNLINK` and `UPLINK` roles.

### `nat_bridge`

Provides routing/NAT through the generic link interface rather than directly depending on Bluetooth PAN or Wi-Fi.

### Event bus

Generic link-state events include:

- `EVENT_DOWNLINK_UP`
- `EVENT_DOWNLINK_DOWN`
- `EVENT_UPLINK_UP`
- `EVENT_UPLINK_DOWN`

The architecture leaves room for future transports such as Bluetooth PPP, Serial/IR, Ethernet and cellular modems. These are not currently implemented as user-facing transports.

---

# Performance

Typical measured values from the current implementation:

| Parameter | Typical value |
| --- | ---: |
| Download | ~0.15–0.20 Mbit/s |
| Upload | ~0.20–0.27 Mbit/s |
| Ping | ~150–200 ms |
| Maximum tested PAN clients | 1 |
| Power draw | ~200 mA from USB |

These figures are not guaranteed. Performance depends strongly on the phone's Bluetooth PAN implementation, RF conditions, Wi-Fi connection and workload.

The measured performance is generally sufficient for lightweight web pages, retro browsers, messaging, J2ME applications, WAP-related software and basic email clients.

---

# Security and pairing

The firmware is intentionally optimized for compatibility with old phones:

- legacy PIN pairing uses **`0000`**;
- SSP user confirmation is automatically accepted.

This is convenient for a dedicated retro-phone gateway, but it is **not a strong security configuration**. Do not expose the bridge to an untrusted environment or treat it as a secure general-purpose access point.

The web interface uses plain HTTP. Do not enter sensitive credentials into it unless you understand the network path and trust the devices that can access the interface.

---

# Project structure

```text
satura-bridge/
├── main/
│   ├── board/
│   ├── main.c
│   ├── app_bootstrap.c
│   ├── app_state.c
│   ├── battery.c
│   ├── bt_pan.c
│   ├── clipboard.c
│   ├── device_name.c
│   ├── dns_server.c
│   ├── event_bus.c
│   ├── http_routes.c
│   ├── http_server.c
│   ├── link_iface.h
│   ├── link_registry.c
│   ├── nat_bridge.c
│   ├── nvs_storage.c
│   ├── proxy_gateway.c
│   ├── proxy_relay.c
│   ├── storage.c
│   ├── watchdog.c
│   ├── wifi_manager.c
│   └── ...
├── components/
├── firmware/
├── sdkconfig.defaults
├── sdkconfig.defaults.generic
├── sdkconfig.defaults.core2
├── sdkconfig.defaults.stickc_plus2
├── build.ps1
├── FLASH.md
├── CHANGELOG.md
└── README.md
```

`build/` contains generated build output and is not part of the source tree.

---

# v0.0.16

v0.0.16 is the current feature and multi-board release.

- Hidden Wi-Fi network support.
- Improved network management and scan presentation.
- Shared `/clip` web clipboard with persistent history and favorites.
- HTTP/HTTPS link detection in clipboard entries.
- Configurable Bluetooth device name through `/name`.
- Improved Bluetooth HCI/BNEP recovery and visibility restoration.
- Improved first-boot NVS handling.
- Fixed saved-network buffer handling.
- Fixed DNS upstream source validation.
- Improved proxy timeout handling.
- Increased HTTP server stack size with stack-headroom monitoring.
- Generic ESP32, original M5Stack Core2 / AXP192 and M5StickC Plus2 board profiles.
- Independent board configurations and reproducible multi-board builds.
- Full merged firmware images.

See [CHANGELOG.md](CHANGELOG.md) for detailed history.

---

# Roadmap

### Networking

- [x] Bluetooth Classic PAN
- [x] Wi-Fi Internet uplink
- [x] NAT / routing
- [x] DNS forwarding
- [x] Connection recovery
- [ ] Bluetooth PPP downlink
- [ ] Ethernet uplink
- [ ] Cellular modem uplink
- [ ] Serial / IR transport

### Legacy-device support

- [x] Sony Ericsson J108 testing
- [x] NetFront 3.4 testing
- [x] J2ME testing
- [ ] More Nokia Symbian devices
- [ ] More Sony Ericsson devices
- [ ] Improved compatibility with older phones
- [ ] Better Bluetooth / Wi-Fi coexistence

### Hardware

- [x] Generic ESP32 board profile
- [x] M5Stack Core2 profile
- [x] M5StickC Plus2 profile
- [ ] Compact custom PCB

---

# Related project

## [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

[Vetera Bridge](https://github.com/arifwn/vetera-bridge) provides Bluetooth PPP Internet connectivity for older S60 v1 devices, including Nokia N-Gage, which use a different networking approach and may require GnuBox.

The distinction is important:

- **Satura Bridge:** Bluetooth PAN → Wi-Fi; no Satura-specific phone software is required when the phone supports PAN.
- **Vetera Bridge:** Bluetooth PPP → Internet; aimed at devices that need PPP/GnuBox.

---

# Contributing and compatibility reports

The most useful contribution is testing on real legacy hardware.

When reporting a compatibility result, include:

- exact phone model;
- firmware / OS version;
- browser or application used;
- whether Bluetooth pairing succeeded;
- whether a PAN connection could be established;
- whether the phone received an IP address;
- whether `http://192.168.7.1` opened;
- whether Internet access worked;
- relevant Bluetooth or Satura Bridge serial logs.

This helps distinguish a phone-side PAN limitation from a bridge-side problem.

Issues and pull requests are welcome.

---

# License

Satura Bridge is released under the **MIT License**.

You may use, modify and redistribute the project under the terms of that license. Keep the original copyright notice as required by the license.

BTstack in `components/btstack/` is distributed under its own license:

```text
components/btstack/LICENSE
```

---

# Author & community

**Author:** [@sigdev](https://github.com/sigildeveloper)

**Community:** [Telegram — @nnmidletschat](https://t.me/nnmidletschat)

**Vintage phones • Symbian • J2ME • Retro networking**

---

# Русская версия

**Satura Bridge — Bluetooth-интернет для старых телефонов.**

Satura Bridge — открытый шлюз на ESP32, который соединяет **Bluetooth Classic PAN** старого телефона с **Wi-Fi** и выполняет NAT/маршрутизацию между ними.

```text
Старый телефон
     │
     │ Bluetooth Classic / PAN
     ▼
Satura Bridge / ESP32
     │
     │ Wi-Fi
     ▼
Роутер → Интернет
```

Телефону не требуется специальное приложение Satura Bridge, если он умеет работать клиентом Bluetooth PAN.

## Что уже работает

- Bluetooth Classic PAN / NAP
- Wi-Fi uplink
- NAT / маршрутизация
- DNS forwarding
- восстановление соединения
- watchdog
- веб-интерфейс настройки
- до 6 сохранённых Wi-Fi сетей
- скрытые SSID
- HTTP proxy и POST
- proxy hostname → IPv4
- общий веб-клипборд
- история и избранное клипборда
- настройка имени Bluetooth
- три профиля плат: Generic ESP32, Core2 и M5StickC Plus2

Протестировано на **Sony Ericsson J108**, старых Android-устройствах, NetFront 3.4, J2ME, Opera Mini и встроенном email-клиенте.

## Важные ограничения

- Сейчас реализован только Bluetooth PAN; **Bluetooth PPP ещё нет**.
- Windows 10 как PAN-клиент Satura Bridge сейчас не поддерживается.
- Максимально проверено: **1 PAN-клиент одновременно**.
- Core2 v1.1 с AXP2101 пока не поддерживается; нужен оригинальный Core2 с AXP192.
- Скорость зависит прежде всего от Bluetooth PAN реализации телефона.

## Быстрый старт

Готовые прошивки находятся в [GitHub Releases](https://github.com/sigildeveloper/satura-bridge/releases).

Для сборки из исходников нужен **ESP-IDF 5.4.x**:

```powershell
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
.\build.ps1
```

Отдельная плата:

```powershell
.\build.ps1 generic
.\build.ps1 core2
.\build.ps1 plus2
```

Прошивка через ESP-IDF, например Core2:

```powershell
idf.py -B build\core2 -p COM6 flash monitor
```

Подробная инструкция: [FLASH.md](FLASH.md).

## Первый запуск

1. Включить Bluetooth на телефоне.
2. Найти **Satura Bridge**.
3. Выполнить pairing.
4. При запросе PIN использовать `0000`.
5. Установить Bluetooth PAN-соединение.
6. Открыть в телефоне `http://192.168.7.1`.
7. Настроить Wi-Fi.

## Веб-интерфейс

```text
http://192.168.7.1/
http://192.168.7.1/networks
http://192.168.7.1/proxy
http://192.168.7.1/clip
http://192.168.7.1/name
```

`192.168.7.1` — адрес Satura Bridge со стороны Bluetooth PAN. Со стороны Wi-Fi/LAN у моста будет другой IP, выданный обычным роутером.

## Клипборд

`/clip` позволяет передавать длинные URL и текст между компьютером и старым телефоном без набора на T9. Хранится 15 последних записей и до 8 избранных.

## Совместимость

| Устройство / ПО | Результат |
| --- | --- |
| Sony Ericsson J108 | Протестировано |
| Старые Android | Работает |
| NetFront 3.4 | Работает |
| J2ME | Работает |
| Opera Mini | Работает |
| Встроенный email | Работает |

При тестировании нового телефона желательно сообщить модель, версию ПО, результат pairing, наличие PAN, получение IP и результат открытия `http://192.168.7.1`.

## Roadmap

- [x] Bluetooth Classic PAN
- [x] Wi-Fi Internet
- [x] NAT / routing
- [x] Web configuration
- [x] Hidden Wi-Fi networks
- [x] HTTP proxy
- [x] Web clipboard
- [x] Bluetooth device name
- [x] Multi-board support
- [ ] Bluetooth PPP
- [ ] Serial / IR
- [ ] Ethernet
- [ ] Cellular modem
- [ ] Больше Nokia/Sony Ericsson устройств
- [ ] Компактная собственная PCB

## Сообщество

**Автор:** [@sigdev](https://github.com/sigildeveloper)

**Telegram:** [@nnmidletschat](https://t.me/nnmidletschat)

**Vintage phones • Symbian • J2ME • Retro networking**
