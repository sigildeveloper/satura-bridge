# Satura Bridge

**Bluetooth PAN → WiFi мост для ретро-телефонов на ESP32**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.9-green.svg)](firmware/)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

## Зачем это нужно

2G и 3G сети закрываются по всему миру. У большинства ретро-телефонов — Nokia S60, Sony Ericsson, и некоторых других — есть Bluetooth, но нет WiFi. Когда операторы отключают 2G, эти телефоны теряют доступ в интернет навсегда.

**Satura Bridge** решает эту проблему. Небольшой модуль ESP32 (~$5) подключается к телефону по Bluetooth и раздаёт интернет через WiFi. Для телефона это выглядит как обычное Bluetooth PAN соединение — никаких драйверов, никаких настроек на стороне телефона.

Не работает с Windows 10, работает со старыми Android девайсами, возможно будет работать с некоторыми КПК и тп.

```
[Ретро-телефон] <--Bluetooth PAN--> [ESP32 Satura Bridge] <--WiFi--> [Роутер] <---> Интернет
```

---

## Возможности

- Работает с любым телефоном поддерживающим **Bluetooth PAN / NAP профиль**
- Протестировано на Sony Ericsson J108, NetFront 3.4, J2ME приложениях
- Веб-интерфейс настройки — открывается автоматически при подключении (captive portal) (Зайдите на любую http:// страницу после первого подключения)
- Автоподключение к WiFi при включении
- Автовосстановление при потере соединения
- Watchdog — перезапускает зависшие компоненты без reboot устройства
- Прошивка одним файлом (firmware/satura-bridge_v0.0.9.bin) через браузер. Прошивать с адреса 0x0, потом перезагрузить устройство.

---

## Железо

| Компонент | Рекомендуемый вариант | Минимальный вариант |
|-----------|----------------------|---------------------|
| Плата | M5Stack Core2 | ESP32 DevKit (с внешней антенной) |
| Питание | USB-C от повербанка | Любой USB 5V |
| Корпус | Встроенный в M5Stack | — |

> Важно: ESP32 с **внешней антенной** работает значительно лучше встроенной. Bluetooth и WiFi работают одновременно и делят радиочастотный тракт.
> Вам понадобится девкит с ESP32 поддерживающей BT Classic. Конкретно в моем случае это был ESP32-D0WDQ6-V3.

---

## Быстрый старт

### 1. Прошить ESP32

Смотри [FLASH.md](FLASH.md) — есть вариант прямо в браузере без установки софта.

### 2. Подключить телефон

1. Включи Bluetooth на телефоне
2. Найди устройство **Satura Bridge** в списке
3. Подключись (PIN если спросит: `0000`)
4. Зайди в браузер
5. Открой любую http:// ссылку или сразу 192.168.7.1

### 3. Настроить WiFi

В открывшейся странице введи имя и пароль своей WiFi сети. Настройки сохраняются в памяти — при следующем включении подключение происходит автоматически.

---

## Веб-интерфейс

Доступен по адресу **http://192.168.7.1** пока телефон подключён по Bluetooth.

| Страница | Описание |
|----------|----------|
| `/` | Статус, RSSI, uptime, heap |
| `/setup` | Настройка WiFi |
| `/reset` | Сбросить сохранённую WiFi сеть |
| `/reboot` | Перезагрузить устройство |

---

## Характеристики

| Параметр | Значение |
|----------|----------|
| Скорость скачивания | ~0.15–0.20 Мбит/с |
| Скорость загрузки | ~0.20–0.27 Мбит/с |
| Пинг | 150–200 мс |
| Максимум клиентов | 1 (DHCP) |
| Потребление | ~200 мА от USB |

Это в нормальных условиях. Старайтесь, чтобы мост располагался между роутером и телефоном.
Скорость ограничена физикой Bluetooth Classic PAN при одновременной работе с WiFi на одном чипе. Для ретро-браузеров, мессенджеров и лёгкого веба — более чем достаточно.

---

## Сборка из исходников

Требуется [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

Компоненты подтягиваются автоматически через `idf_component.yml`.

---

## Структура проекта

```
satura-bridge/
├── main/
│   ├── pan_wifi_bridge.c   # основная логика моста
│   ├── main.c              # точка входа
│   └── btstack_config.h    # конфигурация BTstack
├── components/             # BTstack и зависимости
├── firmware/
│   └── satura_bridge_v0.0.9.bin  # готовый бинарник
├── sdkconfig.defaults      # конфигурация сборки
├── FLASH.md                # инструкция по прошивке
└── README.md
```

---

## Совместимость

Протестировано:

| Устройство | Статус |
|------------|--------|
| NetFront 3.4 | ✅ Работает |
| Android (тест) | ✅ Работает |
| J2ME приложения | ✅ Работает |

Если проверил на своём устройстве — пиши в [чат](https://t.me/nnmidletschat)

---

## Лицензия

MIT — делай что хочешь, упомяни авторство.

BTstack в `components/btstack/` распространяется под своей лицензией (см. `components/btstack/LICENSE`).

---

## Автор и сообщество

Автор: **@sigdev**
Сообщество: [t.me/nnmidletschat](https://t.me/nnmidletschat) — ретро-телефоны, J2ME

---
---

# Satura Bridge (English)

**Bluetooth PAN → WiFi bridge for retro phones on ESP32**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.9-green.svg)](firmware/)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

## Why

2G and 3G networks are being shut down worldwide. Most retro phones — Nokia S60, Sony Ericsson, and others — have Bluetooth but no WiFi. When carriers switch off 2G, these phones permanently lose internet access.

**Satura Bridge** fixes this. A small ESP32 module (~$5) connects to the phone via Bluetooth and provides internet through WiFi. The phone sees it as a regular Bluetooth PAN connection — no drivers, no special setup on the phone side.

Does not work with Windows 10. Works with older Android devices. May work with some PDAs and similar hardware.

```
[Retro phone] <--Bluetooth PAN--> [ESP32 Satura Bridge] <--WiFi--> [Router] <---> Internet
```

---

## Features

- Works with any phone supporting **Bluetooth PAN / NAP profile**
- Tested on Sony Ericsson J108, NetFront 3.4, J2ME apps
- Web setup interface — opens automatically on connect (captive portal). Open any http:// page after connecting
- Auto-connects to saved WiFi on boot
- Auto-recovery on connection loss
- Watchdog — restarts hung components without rebooting the device
- Single-file flashing (firmware/satura-bridge_v0.0.9.bin) via browser. Flash at address 0x0, then reset the device

---

## Hardware

| Component | Recommended | Minimum |
|-----------|-------------|---------|
| Board | M5Stack Core2 | ESP32 DevKit (with external antenna) |
| Power | USB-C powerbank | Any USB 5V |
| Case | Built into M5Stack | — |

> Important: ESP32 with an **external antenna** works significantly better than the built-in one. Bluetooth and WiFi share the same RF front-end and run simultaneously.
> You need an ESP32 board with BT Classic support. In my case: ESP32-D0WDQ6-V3.

---

## Quick Start

### 1. Flash the ESP32

See [FLASH.md](FLASH.md) — includes a browser-based option with no software install.

### 2. Connect your phone

1. Enable Bluetooth on your phone
2. Find **Satura Bridge** in the device list
3. Connect (PIN if asked: `0000`)
4. Open your browser
5. Navigate to any http:// page or directly to 192.168.7.1

### 3. Configure WiFi

Enter your WiFi network name and password in the setup page. Settings are saved to flash — next time the device connects automatically.

---

## Web Interface

Available at **http://192.168.7.1** while a phone is connected via Bluetooth.

| Page | Description |
|------|-------------|
| `/` | Status, RSSI, uptime, heap |
| `/setup` | WiFi configuration |
| `/reset` | Forget saved WiFi network |
| `/reboot` | Reboot the device |

---

## Performance

| Parameter | Value |
|-----------|-------|
| Download | ~0.15–0.20 Mbit/s |
| Upload | ~0.20–0.27 Mbit/s |
| Ping | 150–200 ms |
| Max clients | 1 (DHCP) |
| Power draw | ~200 mA from USB |

Under normal conditions. Try to position the bridge between the router and the phone.
Speed is limited by Bluetooth Classic PAN + WiFi coexistence on a single chip. More than enough for retro browsers, lightweight web, and messaging.

---

## Building from Source

Requires [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

Components are pulled automatically via `idf_component.yml`.

---

## Project Structure

```
satura-bridge/
├── main/
│   ├── pan_wifi_bridge.c   # bridge core logic
│   ├── main.c              # entry point
│   └── btstack_config.h    # BTstack configuration
├── components/             # BTstack and dependencies
├── firmware/
│   └── satura_bridge_v0.0.9.bin  # prebuilt binary
├── sdkconfig.defaults      # build configuration
├── FLASH.md                # flashing instructions
└── README.md
```

---

## Compatibility

Tested:

| Device | Status |
|--------|--------|
| NetFront 3.4 | ✅ Works |
| Android (stress test) | ✅ Works |
| J2ME apps | ✅ Works |

Tested on your device? Post in the [chat](https://t.me/nnmidletschat) and we'll add it to the table.

---

## License

MIT — do whatever you want, mention the author.

BTstack in `components/btstack/` is distributed under its own license (see `components/btstack/LICENSE`).

---

## Author & Community

Author: **@sigdev**
Community: [t.me/nnmidletschat](https://t.me/nnmidletschat) — retro phones, J2ME
