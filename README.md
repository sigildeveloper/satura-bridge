# Satura Bridge

**Bluetooth PAN → WiFi мост для ретро-телефонов на ESP32**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.9-green.svg)](firmware/)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

## Зачем это нужно

2G и 3G сети закрываются по всему миру. У большинства ретро-телефонов — Nokia S60, Sony Ericsson, старых Motorola и других — есть Bluetooth, но нет WiFi. Когда операторы отключают 2G, эти телефоны теряют доступ в интернет навсегда.

**Satura Bridge** решает эту проблему. Небольшой модуль ESP32 (~$5) подключается к телефону по Bluetooth и раздаёт интернет через WiFi. Для телефона это выглядит как обычное Bluetooth PAN соединение — никаких драйверов, никаких настроек на стороне телефона.

```
[Ретро-телефон] <--Bluetooth PAN--> [ESP32 Satura Bridge] <--WiFi--> [Роутер] <---> Интернет
```

---

## Возможности

- Работает с любым телефоном поддерживающим **Bluetooth PAN / NAP профиль**
- Протестировано на Sony Ericsson J108, NetFront 3.4, J2ME приложениях
- Веб-интерфейс настройки — открывается автоматически при подключении (captive portal)
- Автоподключение к WiFi при включении
- Автовосстановление при потере соединения
- Watchdog — перезапускает зависшие компоненты без reboot устройства
- Стабильная работа неделями без обслуживания
- Прошивка одним файлом через браузер

---

## Железо

| Компонент | Рекомендуемый вариант | Минимальный вариант |
|-----------|----------------------|---------------------|
| Плата | M5Stack Core2 | ESP32 DevKit с внешней антенной |
| Питание | USB-C от повербанка | Любой USB 5V |
| Корпус | Встроенный в M5Stack | — |

> Важно: ESP32 с **внешней антенной** работает значительно лучше встроенной. Bluetooth и WiFi работают одновременно и делят радиочастотный тракт.

---

## Быстрый старт

### 1. Прошить ESP32

Смотри [FLASH.md](FLASH.md) — есть вариант прямо в браузере без установки софта.

### 2. Подключить телефон

1. Включи Bluetooth на телефоне
2. Найди устройство **Satura Bridge** в списке
3. Подключись (PIN если спросит: `0000`)
4. Браузер откроется автоматически

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

Скорость ограничена физикой Bluetooth Classic PAN при одновременной работе с WiFi на одном чипе. Для ретро-браузеров, мессенджеров и лёгкого веба — более чем достаточно.

---

## Сборка из исходников

Требуется [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/YOUR_USERNAME/satura-bridge
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

## Why

2G and 3G networks are being shut down worldwide. Most retro phones — Nokia S60, Sony Ericsson, old Motorola devices — have Bluetooth but no WiFi. When carriers switch off 2G, these phones permanently lose internet access.

**Satura Bridge** fixes this. A small ESP32 module (~$5) connects to the phone via Bluetooth and provides internet through WiFi. The phone sees it as a regular Bluetooth PAN connection — no drivers, no special setup on the phone side.

```
[Retro phone] <--Bluetooth PAN--> [ESP32 Satura Bridge] <--WiFi--> [Router] <---> Internet
```

## Quick start

See [FLASH.md](FLASH.md) for flashing instructions including browser-based option with no software install.

**After flashing:**
1. Enable Bluetooth on your phone
2. Find **Satura Bridge** and connect (PIN if asked: `0000`)
3. Browser opens automatically — enter your WiFi credentials
4. Done

## Performance

| Parameter | Value |
|-----------|-------|
| Download | ~0.15–0.20 Mbit/s |
| Upload | ~0.20–0.27 Mbit/s |
| Ping | 150–200 ms |

Speed is limited by Bluetooth Classic PAN + WiFi coexistence on a single chip. Perfectly usable for retro browsers, lightweight web, and messaging apps.

## Community

Chat: [t.me/nnmidletschat](https://t.me/nnmidletschat) — retro phones, J2ME
Author: @sigdev
