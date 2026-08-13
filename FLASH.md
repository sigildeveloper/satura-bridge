# Flashing / Прошивка

## Flashing

### Option 1 — Online Flashing

**No additional software required. Recommended.**

The easiest way to flash Satura Bridge is to use the browser-based ESP Web Tools flasher.

1. Open https://esp.huhn.me in Google Chrome or Microsoft Edge.

2. Connect the ESP32 board to your computer with a USB cable.

3. Click **Connect** and select the serial port of the ESP32.

4. Click **Add File**.

5. Select the firmware file:

   ```text
   firmware/satura-bridge-v0.0.12.bin
   ```

6. Set the flash address to:

   ```text
   0x0
   ```

7. Click **Program**.

8. When flashing is complete, click **Reset** on the device.

> **Important:** The browser flasher requires Web Serial support. Google Chrome and Microsoft Edge are recommended. Firefox does not support Web Serial.

---

### Option 2 — esptool

**Python required.**

Install `esptool`:

```bash
pip install esptool
```

Flash the firmware:

```bash
esptool --port COM3 --baud 460800 write-flash 0x0 firmware/satura-bridge-v0.0.12.bin
```

Replace `COM3` with the serial port of your ESP32.

Examples:

* Windows: `COM3`
* Linux: `/dev/ttyUSB0`
* macOS: `/dev/cu.usbserial-XXXX`

If your version of `esptool` uses the older command syntax, use:

```bash
esptool.py --port COM3 --baud 460800 write_flash 0x0 firmware/satura-bridge-v0.0.12.bin
```

---

### Option 3 — Build from Source

This option requires ESP-IDF v5.4.x.

Clone the repository:

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
```

Build the project:

```bash
idf.py build
```

Flash the ESP32 and open the serial monitor:

```bash
idf.py flash monitor
```

---

## First Boot

After you flash the firmware:

1. Enable Bluetooth on your phone.

2. Find **Satura Bridge** in the Bluetooth device list.

3. Pair and connect the phone to **Satura Bridge**.

4. Open any HTTP URL in the phone browser.

   You can also open:

   ```text
   http://192.168.7.1
   ```

5. The Satura Bridge setup page should appear.

6. Enter the Wi-Fi network name and password.

7. Save the settings.

Satura Bridge connects to the configured Wi-Fi network. It then provides Internet access to the connected phone through Bluetooth PAN.

---

# Прошивка

## Способ 1 — Онлайн-прошивка

**Без дополнительного программного обеспечения. Рекомендуется.**

Самый простой способ прошить Satura Bridge — использовать веб-прошивальщик ESP Web Tools.

1. Откройте https://esp.huhn.me в Google Chrome или Microsoft Edge.

2. Подключите ESP32 к компьютеру с помощью USB-кабеля.

3. Нажмите **Connect** и выберите последовательный порт ESP32.

4. Нажмите **Add File**.

5. Выберите файл прошивки:

   ```text
   firmware/satura-bridge-v0.0.12.bin
   ```

6. Укажите адрес прошивки:

   ```text
   0x0
   ```

7. Нажмите **Program**.

8. После завершения прошивки нажмите **Reset** на устройстве.

> **Важно:** Веб-прошивальщик требует поддержки Web Serial. Рекомендуется использовать Google Chrome или Microsoft Edge. Firefox не поддерживает Web Serial.

---

## Способ 2 — esptool

**Требуется Python.**

Установите `esptool`:

```bash
pip install esptool
```

Прошейте устройство:

```bash
esptool --port COM3 --baud 460800 write-flash 0x0 firmware/satura-bridge-v0.0.12.bin
```

Замените `COM3` на последовательный порт ESP32.

Примеры:

* Windows: `COM3`
* Linux: `/dev/ttyUSB0`
* macOS: `/dev/cu.usbserial-XXXX`

Если ваша версия `esptool` использует старый синтаксис, выполните:

```bash
esptool.py --port COM3 --baud 460800 write_flash 0x0 firmware/satura-bridge-v0.0.12.bin
```

---

## Способ 3 — Сборка из исходного кода

Для этого способа требуется ESP-IDF v5.4.x.

Клонируйте репозиторий:

```bash
git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge
```

Соберите проект:

```bash
idf.py build
```

Прошейте ESP32 и откройте монитор последовательного порта:

```bash
idf.py flash monitor
```

---

## Первый запуск

После прошивки:

1. Включите Bluetooth на телефоне.

2. Найдите **Satura Bridge** в списке Bluetooth-устройств.

3. Выполните сопряжение и подключите телефон к **Satura Bridge**.

4. Откройте любую HTTP-страницу в браузере телефона.

   Можно также открыть:

   ```text
   http://192.168.7.1
   ```

5. Должна открыться страница настройки Satura Bridge.

6. Введите имя и пароль Wi-Fi сети.

7. Сохраните настройки.

Satura Bridge подключится к указанной Wi-Fi сети. После этого Satura Bridge предоставит подключённому телефону доступ в интернет через Bluetooth PAN.
