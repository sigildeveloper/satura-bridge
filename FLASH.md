# Прошивка / Flashing

## Способ 1 — Онлайн, без установки софта (рекомендуется)

1. Открой в Chrome или Edge: **https://esp.huhn.me**
2. Подключи ESP32 по USB
3. Нажми **Connect** → выбери порт устройства
4. Нажми **Add File**, выбери `firmware/satura_bridge_v0.0.9.bin`
5. Адрес укажи **0x0**
6. Нажми **Program**
7. После прошивки нажми **Reset**

> Работает только в Chrome / Edge. Firefox не поддерживает WebSerial.

---

## Способ 2 — esptool (Python)

```bash
pip install esptool
esptool.py --port COM3 --baud 460800 write_flash 0x0 firmware/satura_bridge_v0.0.9.bin
```

Замени `COM3` на свой порт (`/dev/ttyUSB0` на Linux/Mac).

---

## Способ 3 — Сборка из исходников

Требуется: ESP-IDF v5.4.x

```bash
git clone --recursive https://github.com/YOUR_USERNAME/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

---

## После прошивки

1. На телефоне включи Bluetooth
2. Найди устройство **Satura Bridge** и подключись
3. Телефон откроет браузер автоматически (captive portal)
4. Введи имя и пароль своей WiFi сети
5. Готово — интернет работает

---

---

# Flashing (English)

## Option 1 — Online, no software needed (recommended)

1. Open in Chrome or Edge: **https://esp.huhn.me**
2. Connect ESP32 via USB
3. Click **Connect** → select your device port
4. Click **Add File**, select `firmware/satura_bridge_v0.0.9.bin`
5. Set address to **0x0**
6. Click **Program**
7. After flashing click **Reset**

> Chrome / Edge only. Firefox does not support WebSerial.

---

## Option 2 — esptool (Python)

```bash
pip install esptool
esptool.py --port COM3 --baud 460800 write_flash 0x0 firmware/satura_bridge_v0.0.9.bin
```

Replace `COM3` with your port (`/dev/ttyUSB0` on Linux/Mac).

---

## Option 3 — Build from source

Requires: ESP-IDF v5.4.x

```bash
git clone --recursive https://github.com/YOUR_USERNAME/satura-bridge
cd satura-bridge
idf.py build
idf.py flash monitor
```

---

## First boot

1. Enable Bluetooth on your phone
2. Find **Satura Bridge** and connect
3. Browser opens automatically (captive portal)
4. Enter your WiFi network name and password
5. Done — internet is working
