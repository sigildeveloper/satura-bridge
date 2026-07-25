Flashing / Прошивка

Flashing (English)

Option 1 — Online, no software required (recommended)

The easiest way to flash Satura Bridge is using the browser-based ESP Web Tools flasher.

1. Open [https://esp.huhn.me] (https://esp.huhn.me) in Google Chrome or Microsoft Edge.

2. Connect your ESP32 board to your computer via USB.

3. Click Connect and select your ESP32's serial port.

4. Click Add File.

5. Select:
   
   "firmware/satura_bridge_v0.0.9.bin"

6. Set the flash address to:
   
   "0x0"

7. Click Program.

8. After flashing is complete, click Reset on the device.

«Note: The browser flasher requires Web Serial support. Google Chrome and Microsoft Edge are recommended. Firefox does not support Web Serial.»

---

Option 2 — esptool (Python)

Install "esptool":

pip install esptool

Flash the firmware:

esptool --port COM3 --baud 460800 write-flash 0x0 firmware/satura_bridge_v0.0.9.bin

Replace "COM3" with the serial port of your ESP32.

Examples:

- Windows: "COM3"
- Linux: "/dev/ttyUSB0"
- macOS: "/dev/cu.usbserial-XXXX"

If your version of "esptool" uses the older command syntax, use:

esptool.py --port COM3 --baud 460800 write_flash 0x0 firmware/satura_bridge_v0.0.9.bin

---

Option 3 — Build from source

Requires ESP-IDF v5.4.x.

Clone the repository:

git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge

Build the project:

idf.py build

Flash the ESP32 and open the serial monitor:

idf.py flash monitor

---

First Boot

After flashing:

1. Enable Bluetooth on your phone.

2. Find Satura Bridge in the Bluetooth device list.

3. Pair/connect to Satura Bridge.

4. Open any HTTP URL in your phone's browser, or directly open:
   
   "http://192.168.7.1"

5. The Satura Bridge setup page should appear.

6. Enter your Wi-Fi network name and password.

7. Save the settings.

Satura Bridge will connect to the configured Wi-Fi network and provide Internet access to the connected phone over Bluetooth PAN.

---

Прошивка

Способ 1 — Онлайн, без установки дополнительного ПО (рекомендуется)

Самый простой способ прошить Satura Bridge — использовать веб-прошивальщик ESP Web Tools.

1. Откройте [https://esp.huhn.me] (https://esp.huhn.me) в Google Chrome или Microsoft Edge.

2. Подключите ESP32 к компьютеру по USB.

3. Нажмите Connect и выберите последовательный порт ESP32.

4. Нажмите Add File.

5. Выберите файл:
   
   "firmware/satura_bridge_v0.0.9.bin"

6. Укажите адрес прошивки:
   
   "0x0"

7. Нажмите Program.

8. После завершения прошивки нажмите Reset на устройстве.

«Важно: Веб-прошивальщик требует поддержки Web Serial. Рекомендуется использовать Google Chrome или Microsoft Edge. Firefox не поддерживает Web Serial.»

---

Способ 2 — esptool (Python)

Установите "esptool":

pip install esptool

Прошейте устройство:

esptool --port COM3 --baud 460800 write-flash 0x0 firmware/satura_bridge_v0.0.9.bin

Замените "COM3" на последовательный порт вашего ESP32.

Примеры:

- Windows: "COM3"
- Linux: "/dev/ttyUSB0"
- macOS: "/dev/cu.usbserial-XXXX"

Если используется старая версия "esptool", может потребоваться старый синтаксис:

esptool.py --port COM3 --baud 460800 write_flash 0x0 firmware/satura_bridge_v0.0.9.bin

---

Способ 3 — Сборка из исходников

Требуется ESP-IDF v5.4.x.

Клонируйте репозиторий:

git clone --recursive https://github.com/sigildeveloper/satura-bridge
cd satura-bridge

Соберите проект:

idf.py build

Прошейте ESP32 и откройте монитор последовательного порта:

idf.py flash monitor

---

Первый запуск

После прошивки:

1. Включите Bluetooth на телефоне.

2. Найдите Satura Bridge в списке Bluetooth-устройств.

3. Выполните сопряжение и подключитесь к Satura Bridge.

4. Откройте любую HTTP-страницу в браузере телефона или непосредственно перейдите по адресу:
   
   "http://192.168.7.1"

5. Должна открыться страница настройки Satura Bridge.

6. Введите имя и пароль вашей Wi-Fi сети.

7. Сохраните настройки.

Satura Bridge подключится к указанной Wi-Fi сети и предоставит подключённому телефону доступ в интернет через Bluetooth PAN.
