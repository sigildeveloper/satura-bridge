# Satura Bridge

**«Bringing old phones back online.»**

Open-source Bluetooth Classic PAN to Wi-Fi Internet gateway for legacy mobile phones and Symbian devices.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.9-green.svg)](firmware/)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

## What is Satura Bridge?

Satura Bridge is a small ESP32-based Bluetooth Classic PAN to Wi-Fi Internet gateway designed to bring legacy mobile phones back online.

Many older phones, including Nokia Symbian devices, Sony Ericsson phones, and other legacy mobile devices, have Bluetooth but no Wi-Fi. As 2G and 3G networks are being shut down in many parts of the world, these devices are increasingly losing access to mobile Internet.

Satura Bridge provides an alternative: the phone connects to the ESP32 over Bluetooth PAN, while the ESP32 uses a modern Wi-Fi network as its upstream Internet connection.

The phone sees a regular Bluetooth PAN network connection. No drivers or special software are required on the phone side, provided that the device supports the required Bluetooth networking profile.

```text
┌──────────────────┐
│   Legacy phone   │
│  Nokia / Symbian │
└────────┬─────────┘
         │
         │ Bluetooth Classic / PAN
         ▼
┌──────────────────┐
│  Satura Bridge   │
│      ESP32       │
└────────┬─────────┘
         │
         │ Wi-Fi
         ▼
┌──────────────────┐
│      Router      │
└────────┬─────────┘
         │
         ▼
      Internet
```

Satura Bridge is currently focused on devices that support Bluetooth PAN / NAP. Compatibility with individual phones and operating systems may vary and is being tested.

> **Note:** Satura Bridge does not currently work for providing an Internet connection to Windows 10 devices. Older Android devices have been tested successfully. Some PDAs and other Bluetooth Classic devices may also work.

---

## Features

- Bluetooth Classic networking
- Bluetooth PAN / NAP support
- Wi-Fi as the upstream Internet connection
- NAT / routing between Bluetooth and Wi-Fi
- Web-based configuration interface
- Captive portal-style setup
- Automatic connection to saved Wi-Fi on boot
- Automatic connection recovery
- Watchdog that can restart hung components without rebooting the entire device
- Single-file firmware flashing
- Designed with legacy mobile phones and Symbian devices in mind

---

## Hardware

| Component | Recommended | Minimum |
|---|---|---|
| Board | M5Stack Core2 | ESP32-WROOM-32 |
| Power | USB-C power bank | Any USB 5V supply |
| Case | Built into M5Stack Core2 | — |

Satura Bridge requires an ESP32 variant with Bluetooth Classic support.

The current development hardware uses an ESP32-D0WDQ6-V3.

> **Important:** An ESP32 board with an external antenna generally provides significantly better range and reliability than a board with a small onboard antenna. Bluetooth and Wi-Fi operate simultaneously and share the ESP32's RF resources, so antenna placement and overall RF design can affect performance.

A custom hardware revision may be developed in the future.

---

## Quick Start

### 1. Flash the ESP32

See [FLASH.md](FLASH.md) for flashing instructions.

A browser-based flashing option is also available, so no additional software installation is required.

### 2. Connect your phone

1. Enable Bluetooth on your phone.
2. Find **Satura Bridge** in the Bluetooth device list.
3. Pair/connect to it. If a PIN is requested, use `0000`.
4. Open the phone's web browser.
5. Navigate to any `http://` page, or open `http://192.168.7.1` directly.

### 3. Configure Wi-Fi

Enter your Wi-Fi network name and password in the setup page.

The settings are stored in flash memory. On subsequent boots, Satura Bridge will automatically attempt to connect to the saved Wi-Fi network.

---

## Web Interface

The web interface is available at:

**http://192.168.7.1**

It can be accessed while a phone is connected to Satura Bridge over Bluetooth.

| Page | Description |
|---|---|
| `/` | Status, RSSI, uptime, heap information |
| `/setup` | Configure Wi-Fi |
| `/reset` | Forget the saved Wi-Fi network |
| `/reboot` | Reboot the device |

---

## Performance

| Parameter | Value |
|---|---|
| Download | ~0.15–0.20 Mbit/s |
| Upload | ~0.20–0.27 Mbit/s |
| Ping | 150–200 ms |
| Maximum clients | 1 |
| Power draw | ~200 mA from USB |

Measured under normal conditions.

For best results, position Satura Bridge where both the phone and the Wi-Fi router have a good signal.

The current performance is limited by the combination of Bluetooth Classic PAN and Wi-Fi operating simultaneously on a single ESP32, as well as the capabilities of the Bluetooth PAN connection itself.

For retro browsers, lightweight web pages, messaging, and similar tasks, the available performance is generally sufficient.

---

## Compatibility

Satura Bridge is primarily intended for legacy mobile phones and other devices that support Bluetooth Classic networking.

### Tested

| Device / Platform | Result |
|---|---|
| Sony Ericsson J108 | Tested |
| Older Android devices | ✅ Working |
| NetFront 3.4 | ✅ Working |
| J2ME applications | ✅ Working |

Compatibility with individual devices may vary depending on their Bluetooth PAN implementation and operating system.

If you test Satura Bridge on a device that is not listed here, please share the result in the [community chat](https://t.me/nnmidletschat).

Useful information to include:

- Device model
- Operating system and version
- Bluetooth profile support, if known
- Whether pairing was successful
- Whether a PAN connection was established
- Whether web browsing worked

---

## Roadmap

- [x] Bluetooth Classic PAN
- [x] Wi-Fi Internet connectivity
- [x] NAT / routing
- [x] Web-based configuration
- [x] Automatic Wi-Fi connection
- [x] Connection recovery
- [x] Watchdog and recovery mechanisms
- [ ] Test more Nokia Symbian devices
- [ ] Test more Sony Ericsson devices
- [ ] Improve compatibility with older phones
- [ ] Improve Bluetooth / Wi-Fi coexistence
- [ ] Develop a compact custom PCB with two ESP32 modules onboard
- [ ] Explore Ethernet connectivity

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

```text
satura-bridge/
├── main/
│   ├── pan_wifi_bridge.c        # Core gateway logic
│   ├── main.c                   # Entry point
│   └── btstack_config.h         # BTstack configuration
├── components/                  # BTstack and dependencies
├── firmware/
│   └── satura_bridge_v0.0.9.bin # Prebuilt firmware
├── sdkconfig.defaults           # Build configuration
├── FLASH.md                     # Flashing instructions
└── README.md
```

---

## Related Projects

### [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge is a project that provides Internet connectivity over Bluetooth PPP for older S60 v1 devices, including devices such as the Nokia N-Gage, that do not support Bluetooth PAN.

It uses GnuBox on the phone side.

Satura Bridge and Vetera Bridge target different generations and networking capabilities of legacy mobile phones.

---

## License

Satura Bridge is released under the MIT License.

Do whatever you want with the project, but please keep the original attribution.

BTstack in `components/btstack/` is distributed under its own license. See [`components/btstack/LICENSE`](components/btstack/LICENSE).

---

## Author & Community

**Author:** [@sigdev](https://github.com/sigildeveloper)

**Community:** [Telegram — @nnmidletschat](https://t.me/nnmidletschat)

**Retro phones • Symbian • J2ME • Bluetooth networking**

---

# Русская версия

**«Возвращаем старые телефоны в интернет.»**

Satura Bridge — Bluetooth PAN → Wi-Fi шлюз для старых мобильных телефонов и устройств на Symbian.

---

## Что такое Satura Bridge?

Satura Bridge — небольшой шлюз на базе ESP32, который позволяет старым мобильным телефонам выходить в интернет через Bluetooth, используя современную Wi-Fi сеть в качестве подключения к интернету.

Многие старые телефоны, включая Nokia, Sony Ericsson и другие устройства, имеют Bluetooth, но не имеют Wi-Fi. При этом 2G и 3G сети во многих странах постепенно отключаются, и такие устройства теряют возможность пользоваться мобильным интернетом.

Satura Bridge предлагает альтернативу: телефон подключается к ESP32 по Bluetooth PAN, а ESP32 подключается к роутеру по Wi-Fi.

Для телефона это выглядит как обычное Bluetooth PAN-соединение. Никаких драйверов или специального программного обеспечения на стороне телефона не требуется, если устройство поддерживает необходимый Bluetooth-профиль.

```text
┌──────────────────┐
│  Старый телефон  │
│  Nokia / Symbian │
└────────┬─────────┘
         │
         │ Bluetooth Classic / PAN
         ▼
┌──────────────────┐
│  Satura Bridge   │
│      ESP32       │
└────────┬─────────┘
         │
         │ Wi-Fi
         ▼
┌──────────────────┐
│      Роутер      │
└────────┬─────────┘
         │
         ▼
      Интернет
```

Satura Bridge в первую очередь рассчитан на устройства с поддержкой Bluetooth PAN / NAP. Совместимость с конкретными телефонами и версиями ОС может различаться и сейчас активно тестируется.

> **Важно:** В текущей версии Satura Bridge не работает для предоставления интернет-соединения устройствам под управлением Windows 10. Старые Android-устройства были успешно протестированы. Некоторые КПК и другие устройства с Bluetooth Classic также могут работать.

---

## Возможности

- Bluetooth Classic
- Bluetooth PAN / NAP
- Wi-Fi в качестве подключения к интернету
- NAT / маршрутизация между Bluetooth и Wi-Fi
- Веб-интерфейс настройки
- Captive portal для первоначальной настройки
- Автоматическое подключение к сохранённой Wi-Fi сети после включения
- Автоматическое восстановление соединения
- Watchdog, способный перезапускать зависшие компоненты без полной перезагрузки устройства
- Прошивка одним файлом
- Ориентация на старые мобильные телефоны и устройства Symbian

---

## Железо

| Компонент | Рекомендуемый вариант | Минимальный вариант |
|---|---|---|
| Плата | M5Stack Core2 | ESP32-WROOM-32 |
| Питание | USB-C повербанк | Любой источник USB 5V |
| Корпус | Встроенный корпус M5Stack Core2 | — |

Для работы Satura Bridge требуется ESP32 с поддержкой Bluetooth Classic.

Текущее тестовое устройство использует ESP32-D0WDQ6-V3.

> **Важно:** ESP32 с внешней антенной обычно обеспечивает значительно лучшую дальность и стабильность соединения по сравнению с платами с небольшой встроенной антенной. Bluetooth и Wi-Fi работают одновременно и используют общие радиочастотные ресурсы ESP32, поэтому расположение антенны и качество RF-разводки могут заметно влиять на результат.

В будущем планируется разработка собственной компактной платы.

---

## Быстрый старт

### 1. Прошить ESP32

Смотри [`FLASH.md`](FLASH.md) — там есть инструкция по прошивке.

Также доступен вариант прошивки прямо через браузер без установки дополнительного программного обеспечения.

### 2. Подключить телефон

1. Включите Bluetooth на телефоне.
2. Найдите **Satura Bridge** в списке Bluetooth-устройств.
3. Выполните сопряжение. Если запрашивается PIN, используйте `0000`.
4. Откройте браузер на телефоне.
5. Перейдите на любую страницу `http://` или непосредственно на `http://192.168.7.1`.

### 3. Настроить Wi-Fi

Введите название и пароль вашей Wi-Fi сети в открывшейся странице настройки.

Настройки сохраняются во flash-памяти. При последующем включении Satura Bridge автоматически пытается подключиться к сохранённой Wi-Fi сети.

---

## Веб-интерфейс

Веб-интерфейс доступен по адресу:

**http://192.168.7.1**

Он доступен, пока телефон подключён к Satura Bridge по Bluetooth.

| Страница | Описание |
|---|---|
| `/` | Статус, RSSI, время работы, информация о heap |
| `/setup` | Настройка Wi-Fi |
| `/reset` | Удаление сохранённой Wi-Fi сети |
| `/reboot` | Перезагрузка устройства |

---

## Производительность

| Параметр | Значение |
|---|---|
| Скорость скачивания | ~0.15–0.20 Мбит/с |
| Скорость загрузки | ~0.20–0.27 Мбит/с |
| Пинг | 150–200 мс |
| Максимальное количество клиентов | 1 |
| Потребление | ~200 мА от USB |

Измерения получены в нормальных условиях.

Для лучшего результата располагайте Satura Bridge так, чтобы и телефон, и Wi-Fi роутер имели хороший уровень сигнала.

Производительность ограничена одновременной работой Bluetooth Classic и Wi-Fi на одном ESP32, а также возможностями самого Bluetooth PAN-соединения.

Для ретро-браузеров, лёгких веб-страниц, обмена сообщениями и подобных задач производительности обычно достаточно.

---

## Совместимость

Satura Bridge в первую очередь рассчитан на старые мобильные телефоны и другие устройства с поддержкой Bluetooth Classic networking.

### Протестировано

| Устройство / платформа | Результат |
|---|---|
| Sony Ericsson J108 | Протестировано |
| Старые Android-устройства | ✅ Работает |
| NetFront 3.4 | ✅ Работает |
| J2ME-приложения | ✅ Работает |

Совместимость с конкретными устройствами может зависеть от реализации Bluetooth PAN и используемой операционной системы.

Если вы протестировали Satura Bridge на устройстве, которого нет в списке, напишите результат в [сообщество Telegram](https://t.me/nnmidletschat).

Желательно указать:

- модель устройства;
- операционную систему и версию;
- поддержку Bluetooth-профилей, если известна;
- удалось ли выполнить сопряжение;
- удалось ли установить PAN-соединение;
- работает ли веб-браузер.

---

## Планы

- [x] Bluetooth Classic PAN
- [x] Wi-Fi подключение к интернету
- [x] NAT / маршрутизация
- [x] Веб-интерфейс настройки
- [x] Автоматическое подключение к Wi-Fi
- [x] Восстановление соединения
- [x] Watchdog и механизмы восстановления
- [ ] Протестировать больше устройств Nokia на Symbian
- [ ] Протестировать больше устройств Sony Ericsson
- [ ] Улучшить совместимость со старыми телефонами
- [ ] Улучшить совместную работу Bluetooth и Wi-Fi
- [ ] Разработать компактную собственную PCB под два модуля ESP32
- [ ] Исследовать поддержку Ethernet

---

## Сборка из исходников

Требуется [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

Компоненты автоматически подтягиваются через `idf_component.yml`.

---

## Структура проекта

```text
satura-bridge/
├── main/
│   ├── pan_wifi_bridge.c        # Основная логика шлюза
│   ├── main.c                   # Точка входа
│   └── btstack_config.h         # Конфигурация BTstack
├── components/                  # BTstack и зависимости
├── firmware/
│   └── satura_bridge_v0.0.9.bin # Готовая прошивка
├── sdkconfig.defaults           # Конфигурация сборки
├── FLASH.md                     # Инструкция по прошивке
└── README.md
```

---

## Похожие проекты

### [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge — отдельный проект, который обеспечивает доступ к интернету через Bluetooth PPP для старых устройств S60 v1, включая Nokia N-Gage и другие аппараты, не поддерживающие Bluetooth PAN.

На стороне телефона используется GnuBox.

Satura Bridge и Vetera Bridge рассчитаны на разные поколения старых телефонов и используют разные сетевые технологии.

---

## Лицензия

Satura Bridge распространяется под лицензией MIT.

Делайте с проектом что хотите, но, пожалуйста, сохраняйте указание авторства.

BTstack в `components/btstack/` распространяется под собственной лицензией. См. [`components/btstack/LICENSE`](components/btstack/LICENSE).

---

## Автор и сообщество

**Автор:** [@sigdev](https://github.com/sigildeveloper)

**Сообщество:** [Telegram — @nnmidletschat](https://t.me/nnmidletschat)

**Ретро-телефоны • Symbian • J2ME • Bluetooth networking**