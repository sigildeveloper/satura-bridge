# Satura Bridge

**«Bringing old phones back online.»**

Open-source Bluetooth Classic PAN to Wi-Fi Internet gateway for legacy mobile phones and Symbian devices.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-v0.0.12-green.svg)](firmware/)
[![Community](https://img.shields.io/badge/Telegram-nnmidletschat-blue?logo=telegram)](https://t.me/nnmidletschat)

---

## What is Satura Bridge?

Satura Bridge is a small ESP32-based Bluetooth Classic PAN to Wi-Fi Internet gateway. It provides Internet access for legacy mobile phones.

Many older phones, including Nokia Symbian devices, Sony Ericsson phones, and other legacy mobile devices, have Bluetooth but do not have Wi-Fi. In many parts of the world, 2G and 3G networks are being shut down. As a result, these devices can lose mobile Internet access.

Satura Bridge provides an alternative connection. The phone connects to Satura Bridge through Bluetooth PAN. Satura Bridge connects to a Wi-Fi network for Internet access.

The phone sees a standard Bluetooth PAN network connection. The phone does not need drivers or special software if it supports the required Bluetooth networking profile.

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

Satura Bridge is designed mainly for devices that support Bluetooth PAN / NAP. Compatibility can be different for different phones and operating systems. Compatibility testing is in progress.

> **Note:** Satura Bridge does not currently provide an Internet connection to Windows 10 devices. Older Android devices have been tested successfully. Some PDAs and other Bluetooth Classic devices can also work.

---

## Features

* Bluetooth Classic networking
* Bluetooth PAN / NAP support
* Wi-Fi as the upstream Internet connection
* NAT / routing between Bluetooth and Wi-Fi
* Web-based configuration interface
* Captive portal-style setup
* Automatic connection to a saved Wi-Fi network at boot
* Support for up to 6 saved Wi-Fi networks, with connection to the network with the strongest visible signal
* HTTP proxy gateway support, for example WAP compression gateways
* Automatic connection recovery
* Watchdog that can restart stopped components without restarting the complete device
* Single-file firmware flashing
* Support for legacy mobile phones and Symbian devices

---

## Hardware

| Component | Recommended      | Minimum                  |
| --------- | ---------------- | ------------------------ |
| Board     | M5Stack Core2    | ESP32-WROOM-32           |
| Power     | Built-in battery | Any USB 5V supply        |
| Case      | Already in case  | Optional 3D-printed case |

Satura Bridge requires an ESP32 variant with Bluetooth Classic support.

The current development hardware uses an ESP32-D0WDQ6-V3.

> **Important:** An ESP32 board with an external antenna can provide better range and connection stability than a board with a small onboard antenna. Bluetooth and Wi-Fi operate at the same time and use the ESP32 RF resources. Antenna position and RF design can therefore affect performance.

A custom hardware revision may be developed in the future.

---

## Quick Start

### 1. Flash the ESP32

See [FLASH.md](FLASH.md) for flashing instructions.

A browser-based flashing option is also available. It does not require additional software.

### 2. Connect your phone

1. Enable Bluetooth on the phone.
2. Find **Satura Bridge** in the Bluetooth device list.
3. Pair the phone with Satura Bridge. If the phone requests a PIN, enter `0000`.
4. Open the web browser on the phone.
5. Open any `http://` page, or open `http://192.168.7.1` directly.

### 3. Configure Wi-Fi

Enter the Wi-Fi network name and password in the setup page.

Satura Bridge stores the settings in flash memory. At the next boot, Satura Bridge automatically tries to connect to the saved Wi-Fi network.

---

## Web Interface

The web interface is available at:

**http://192.168.7.1**

You can access the web interface while a phone is connected to Satura Bridge through Bluetooth.

| Page        | Description                                      |
| ----------- | ------------------------------------------------ |
| `/`         | Shows status, RSSI, uptime, and heap information |
| `/setup`    | Configures Wi-Fi manually                        |
| `/networks` | Scans, adds, and removes saved Wi-Fi networks    |
| `/proxy`    | Configures the HTTP proxy                        |
| `/reset`    | Removes all saved Wi-Fi networks                 |
| `/reboot`   | Restarts the device                              |

---

## Performance

| Parameter       | Value             |
| --------------- | ----------------- |
| Download        | ~0.15–0.20 Mbit/s |
| Upload          | ~0.20–0.27 Mbit/s |
| Ping            | 150–200 ms        |
| Maximum clients | 1                 |
| Power draw      | ~200 mA from USB  |

These values were measured under normal conditions.

For best results, put Satura Bridge where the phone and the Wi-Fi router have a good signal.

Performance is limited by the simultaneous use of Bluetooth Classic PAN and Wi-Fi on one ESP32. The Bluetooth PAN connection also limits performance.

The available performance is normally sufficient for retro browsers, lightweight web pages, messaging, and similar tasks.

---

## Compatibility

Satura Bridge is designed mainly for legacy mobile phones and other devices that support Bluetooth Classic networking.

### Tested

| Device / Platform              | Result    |
| ------------------------------ | --------- |
| Sony Ericsson J108             | Tested    |
| Older Android devices          | ✅ Working |
| NetFront 3.4                   | ✅ Working |
| J2ME applications (Opera Mini) | ✅ Working |
| Built-in email client          | ✅ Working |

Compatibility can be different for different devices. It depends on the Bluetooth PAN implementation and the operating system.

If you test Satura Bridge on a device that is not in this list, please share the result in the [community chat](https://t.me/nnmidletschat).

Useful information to include:

* Device model
* Operating system and version
* Bluetooth profile support, if known
* Whether pairing was successful
* Whether a PAN connection was established
* Whether web browsing worked

---

## Roadmap

* [x] Bluetooth Classic PAN
* [x] Wi-Fi Internet connectivity
* [x] NAT / routing
* [x] Web-based configuration
* [x] Automatic Wi-Fi connection
* [x] Connection recovery
* [x] Watchdog and recovery mechanisms
* [x] Modular codebase refactor
* [x] Multiple saved Wi-Fi networks with scan-and-connect UI
* [x] HTTP proxy gateway support
* [ ] Unified Wi-Fi connection state machine (queue-based, single owner)
* [ ] Proxy: resolve gateway hostnames, not only IP addresses
* [ ] Proxy: forward POST request bodies
* [ ] Test more Nokia Symbian devices
* [ ] Test more Sony Ericsson devices
* [ ] Improve compatibility with older phones
* [ ] Improve Bluetooth / Wi-Fi coexistence
* [ ] Develop a compact custom PCB with two ESP32 modules onboard
* [ ] Explore Ethernet connectivity

---

## Building from Source

Requires [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

Components are downloaded automatically through `idf_component.yml`.

---

## Project Structure

```text
satura-bridge/
├── main/
│   ├── pan_wifi_bridge.c        # Core coordination (NAT trigger, watchdog, entry glue)
│   ├── main.c                   # Entry point
│   ├── app_state.c/.h           # Bridge state machine
│   ├── wifi_manager.c/.h        # Wi-Fi connection, retry, recovery
│   ├── proxy_gateway.c/.h       # Proxy
│   ├── bt_pan.c/.h              # Bluetooth PAN / BNEP handling
│   ├── nat_bridge.c/.h          # NAT between BT and Wi-Fi interfaces
│   ├── dns_server.c/.h          # DNS server with caching and captive replies
│   ├── http_server.c/.h         # Web interface (setup, status, captive portal)
│   ├── nvs_storage.c/.h         # Wi-Fi credential storage
│   ├── http_utils.c/.h          # Shared HTTP helpers
│   ├── uptime.c/.h              # Uptime tracking
│   ├── config.h                 # Shared constants
│   └── btstack_config.h         # BTstack configuration
├── components/                  # BTstack and dependencies
├── firmware/
│   └── satura-bridge-v0.0.12.bin # Prebuilt firmware
├── sdkconfig.defaults           # Build configuration
├── FLASH.md                     # Flashing instructions
└── README.md
```

---

## Related Projects

### [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge provides Internet connectivity through Bluetooth PPP for older S60 v1 devices, including devices such as the Nokia N-Gage that do not support Bluetooth PAN.

It uses GnuBox on the phone.

Satura Bridge and Vetera Bridge target different generations of legacy mobile phones and use different networking technologies.

---

## License

Satura Bridge is released under the MIT License.

You can use and modify the project as you want. Keep the original attribution.

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

Satura Bridge — небольшой шлюз на базе ESP32. Он позволяет старым мобильным телефонам получать доступ в интернет через Bluetooth. Для подключения к интернету Satura Bridge использует Wi-Fi сеть.

Многие старые телефоны, включая Nokia, Sony Ericsson и другие устройства, имеют Bluetooth, но не имеют Wi-Fi. Во многих странах сети 2G и 3G постепенно отключаются. Поэтому такие устройства могут потерять доступ к мобильному интернету.

Satura Bridge предоставляет другой способ подключения. Телефон подключается к Satura Bridge через Bluetooth PAN. Satura Bridge подключается к роутеру через Wi-Fi.

Для телефона Satura Bridge выглядит как обычное Bluetooth PAN-соединение. Драйверы и специальное программное обеспечение на телефоне не требуются, если телефон поддерживает необходимый Bluetooth-профиль.

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

Satura Bridge в первую очередь предназначен для устройств с поддержкой Bluetooth PAN / NAP. Совместимость может различаться в зависимости от телефона и операционной системы. Тестирование совместимости продолжается.

> **Важно:** Satura Bridge в текущей версии не предоставляет интернет-соединение устройствам под управлением Windows 10. Старые Android-устройства были успешно протестированы. Некоторые КПК и другие устройства с Bluetooth Classic также могут работать.

---

## Возможности

* Bluetooth Classic
* Bluetooth PAN / NAP
* Wi-Fi для подключения к интернету
* NAT / маршрутизация между Bluetooth и Wi-Fi
* Веб-интерфейс настройки
* Captive portal для первоначальной настройки
* Автоматическое подключение к сохранённой Wi-Fi сети после включения
* Поддержка до 6 сохранённых Wi-Fi сетей с подключением к сети с самым сильным видимым сигналом
* Поддержка HTTP прокси, например прокси для сжатия трафика WAP
* Автоматическое восстановление соединения
* Watchdog, который может перезапускать остановленные компоненты без полной перезагрузки устройства
* Прошивка одним файлом
* Поддержка старых мобильных телефонов и устройств Symbian

---

## Железо

| Компонент | Рекомендуемый вариант | Минимальный вариант                                         |
| --------- | --------------------- | ----------------------------------------------------------- |
| Плата     | M5Stack Core2         | ESP32-WROOM-32                                              |
| Питание   | Встроенная батарея    | Любой источник USB 5V                                       |
| Корпус    | Уже установлен        | Корпус не обязателен; можно использовать 3D-печатный корпус |

Для работы Satura Bridge требуется ESP32 с поддержкой Bluetooth Classic.

Текущее тестовое устройство использует ESP32-D0WDQ6-V3.

> **Важно:** ESP32 с внешней антенной может обеспечивать большую дальность и более стабильное соединение по сравнению с платой с небольшой встроенной антенной. Bluetooth и Wi-Fi работают одновременно и используют радиоресурсы ESP32. Поэтому положение антенны и конструкция RF-части могут влиять на работу устройства.

В будущем может быть разработана собственная версия платы.

---

## Быстрый старт

### 1. Прошить ESP32

См. [`FLASH.md`](FLASH.md) для получения инструкций по прошивке.

Также доступна прошивка через браузер. Дополнительное программное обеспечение для этого не требуется.

### 2. Подключить телефон

1. Включите Bluetooth на телефоне.
2. Найдите **Satura Bridge** в списке Bluetooth-устройств.
3. Выполните сопряжение с Satura Bridge. Если телефон запрашивает PIN, введите `0000`.
4. Откройте браузер на телефоне.
5. Откройте любую страницу `http://` или непосредственно `http://192.168.7.1`.

### 3. Настроить Wi-Fi

Введите имя и пароль Wi-Fi сети на странице настройки.

Satura Bridge сохраняет настройки во flash-памяти. При следующем включении Satura Bridge автоматически пытается подключиться к сохранённой Wi-Fi сети.

---

## Веб-интерфейс

Веб-интерфейс доступен по адресу:

**http://192.168.7.1**

Веб-интерфейс можно открыть, когда телефон подключён к Satura Bridge через Bluetooth.

| Страница    | Описание                                                  |
| ----------- | --------------------------------------------------------- |
| `/`         | Показывает статус, RSSI, время работы и информацию о heap |
| `/setup`    | Настраивает Wi-Fi вручную                                 |
| `/networks` | Сканирует, добавляет и удаляет сохранённые Wi-Fi сети     |
| `/proxy`    | Настраивает HTTP прокси                                   |
| `/reset`    | Удаляет все сохранённые Wi-Fi сети                        |
| `/reboot`   | Перезапускает устройство                                  |

---

## Производительность

| Параметр                         | Значение          |
| -------------------------------- | ----------------- |
| Скорость скачивания              | ~0.15–0.20 Мбит/с |
| Скорость загрузки                | ~0.20–0.27 Мбит/с |
| Пинг                             | 150–200 мс        |
| Максимальное количество клиентов | 1                 |
| Потребление                      | ~200 мА от USB    |

Значения получены в нормальных условиях.

Для лучшего результата установите Satura Bridge в месте с хорошим сигналом телефона и Wi-Fi роутера.

Производительность ограничена одновременной работой Bluetooth Classic PAN и Wi-Fi на одном ESP32. Возможности Bluetooth PAN-соединения также ограничивают производительность.

Для ретро-браузеров, лёгких веб-страниц, обмена сообщениями и подобных задач доступной производительности обычно достаточно.

---

## Совместимость

Satura Bridge в первую очередь предназначен для старых мобильных телефонов и других устройств с поддержкой Bluetooth Classic networking.

### Протестировано

| Устройство / платформа    | Результат      |
| ------------------------- | -------------- |
| Sony Ericsson J108        | Протестировано |
| Старые Android-устройства | ✅ Работает     |
| NetFront 3.4              | ✅ Работает     |
| J2ME-приложения           | ✅ Работает     |

Совместимость может различаться в зависимости от устройства. Она зависит от реализации Bluetooth PAN и используемой операционной системы.

Если вы протестировали Satura Bridge на устройстве, которого нет в списке, сообщите результат в [сообщество Telegram](https://t.me/nnmidletschat).

Желательно указать:

* модель устройства;
* операционную систему и версию;
* поддержку Bluetooth-профилей, если она известна;
* удалось ли выполнить сопряжение;
* удалось ли установить PAN-соединение;
* работает ли веб-браузер.

---

## Планы

* [x] Bluetooth Classic PAN
* [x] Wi-Fi подключение к интернету
* [x] NAT / маршрутизация
* [x] Веб-интерфейс настройки
* [x] Автоматическое подключение к Wi-Fi
* [x] Восстановление соединения
* [x] Watchdog и механизмы восстановления
* [x] Модульный рефакторинг кодовой базы
* [x] Несколько сохранённых Wi-Fi сетей с UI сканирования и подключения
* [x] Поддержка HTTP прокси
* [ ] Единый конечный автомат для Wi-Fi подключений (с очередью команд и одним владельцем)
* [ ] Прокси: разрешение имён шлюза, а не только IP-адресов
* [ ] Прокси: пересылка тела POST-запросов
* [ ] Протестировать больше устройств Nokia на Symbian
* [ ] Протестировать больше устройств Sony Ericsson
* [ ] Улучшить совместимость со старыми телефонами
* [ ] Улучшить совместную работу Bluetooth и Wi-Fi
* [ ] Разработать компактную собственную PCB с двумя модулями ESP32
* [ ] Исследовать поддержку Ethernet

---

## Сборка из исходников

Требуется [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

Компоненты автоматически загружаются через `idf_component.yml`.

---

## Структура проекта

```text
satura-bridge/
├── main/
│   ├── pan_wifi_bridge.c        # Основная координация (запуск NAT, watchdog, точка входа)
│   ├── main.c                   # Точка входа
│   ├── app_state.c/.h           # Конечный автомат состояния моста
│   ├── wifi_manager.c/.h        # Подключение, повторные попытки и восстановление Wi-Fi
│   ├── proxy_gateway.c/.h       # Прокси
│   ├── bt_pan.c/.h              # Обработка Bluetooth PAN / BNEP
│   ├── nat_bridge.c/.h          # NAT между интерфейсами BT и Wi-Fi
│   ├── dns_server.c/.h          # DNS-сервер с кэшем и captive-ответами
│   ├── http_server.c/.h         # Веб-интерфейс (настройка, статус, captive portal)
│   ├── nvs_storage.c/.h         # Сохранение данных Wi-Fi
│   ├── http_utils.c/.h          # Общие HTTP-функции
│   ├── uptime.c/.h              # Учёт времени работы
│   ├── config.h                 # Общие константы
│   └── btstack_config.h         # Конфигурация BTstack
├── components/                  # BTstack и зависимости
├── firmware/
│   └── satura-bridge-v0.0.12.bin # Готовая прошивка
├── sdkconfig.defaults           # Конфигурация сборки
├── FLASH.md                     # Инструкция по прошивке
└── README.md
```

---

## Похожие проекты

### [Vetera Bridge](https://github.com/arifwn/vetera-bridge)

Vetera Bridge предоставляет доступ к интернету через Bluetooth PPP для старых устройств S60 v1, включая Nokia N-Gage и другие устройства без поддержки Bluetooth PAN.

На телефоне используется GnuBox.

Satura Bridge и Vetera Bridge предназначены для разных поколений старых мобильных телефонов и используют разные сетевые технологии.

---

## Лицензия

Satura Bridge распространяется под лицензией MIT.

Вы можете использовать и изменять проект. Сохраняйте исходное указание авторства.

BTstack в `components/btstack/` распространяется под собственной лицензией. См. [`components/btstack/LICENSE`](components/btstack/LICENSE).

---

## Автор и сообщество

**Автор:** [@sigdev](https://github.com/sigildeveloper)

**Сообщество:** [Telegram — @nnmidletschat](https://t.me/nnmidletschat)

**Ретро-телефоны • Symbian • J2ME • Bluetooth networking**
