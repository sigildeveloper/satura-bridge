# Flashing / Прошивка

Satura Bridge поддерживает несколько аппаратных профилей. Для каждой платы используется собственная конфигурация и собственный полный firmware image.

## Supported boards

| Board                | Flash | Firmware                                                    |
| -------------------- | ----- | ----------------------------------------------------------- |
| Generic ESP32 DevKit | 2 MB  | `firmware/generic/satura-bridge-generic-full.bin`           |
| M5Stack Core2        | 16 MB | `firmware/core2/satura-bridge-core2-full.bin`               |
| M5StickC Plus2       | 8 MB  | `firmware/stickc_plus2/satura-bridge-stickc_plus2-full.bin` |

---

# Option 1 — ESP-IDF

**Recommended for development.**

ESP-IDF v5.4.x is required.

The repository contains an independent build directory for each board.

## M5Stack Core2

Build the Core2 firmware:

```powershell
.\build.ps1 core2
```

Flash it to COM6:

```powershell
idf.py -B build\core2 -p COM6 flash
```

Flash and open the serial monitor:

```powershell
idf.py -B build\core2 -p COM6 flash monitor
```

Replace `COM6` with the actual serial port of the board.

## Generic ESP32

```powershell
.\build.ps1 generic
idf.py -B build\generic -p COM3 flash
```

## M5StickC Plus2

```powershell
.\build.ps1 plus2
idf.py -B build\stickc_plus2 -p COM3 flash
```

---

# Option 2 — Full firmware image with esptool

The build script creates a merged firmware image containing the bootloader, partition table and application.

The merged image is designed to be flashed at address `0x0`.

## M5Stack Core2

```powershell
python -m esptool --chip esp32 --port COM6 --baud 460800 write-flash 0x0 firmware\core2\satura-bridge-core2-full.bin
```

## Generic ESP32

```powershell
python -m esptool --chip esp32 --port COM3 --baud 460800 write-flash 0x0 firmware\generic\satura-bridge-generic-full.bin
```

## M5StickC Plus2

```powershell
python -m esptool --chip esp32 --port COM3 --baud 460800 write-flash 0x0 firmware\stickc_plus2\satura-bridge-stickc_plus2-full.bin
```

Replace the COM port with the port assigned to your board.

If your installed esptool uses the older command syntax:

```powershell
esptool.py --port COM6 --baud 460800 write_flash 0x0 firmware\core2\satura-bridge-core2-full.bin
```

---

# Option 3 — Browser-based flashing

A merged full firmware image can also be flashed using an ESP32-compatible Web Serial flasher.

Recommended browsers:

* Google Chrome
* Microsoft Edge

Firefox does not currently provide the required Web Serial support.

## M5Stack Core2

Use:

```text
firmware/core2/satura-bridge-core2-full.bin
```

Flash address:

```text
0x0
```

## Generic ESP32

Use:

```text
firmware/generic/satura-bridge-generic-full.bin
```

Flash address:

```text
0x0
```

## M5StickC Plus2

Use:

```text
firmware/stickc_plus2/satura-bridge-stickc_plus2-full.bin
```

Flash address:

```text
0x0
```

---

# Building firmware

The recommended build command is:

```powershell
.\build.ps1
```

This builds all supported boards.

The output is:

```text
firmware/
├── generic/
│   └── satura-bridge-generic-full.bin
├── core2/
│   └── satura-bridge-core2-full.bin
└── stickc_plus2/
    └── satura-bridge-stickc_plus2-full.bin
```

To build only one board:

```powershell
.\build.ps1 generic
.\build.ps1 core2
.\build.ps1 plus2
```

For a complete clean rebuild:

```powershell
.\build.ps1 rebuild
```

For one board:

```powershell
.\build.ps1 rebuild core2
```

The build script does not flash any board.

---

# ESP-IDF menuconfig

Each board has its own independent configuration.

Generic:

```powershell
idf.py -B build\generic menuconfig
```

M5Stack Core2:

```powershell
idf.py -B build\core2 menuconfig
```

M5StickC Plus2:

```powershell
idf.py -B build\stickc_plus2 menuconfig
```

The resulting configurations are stored in:

```text
build/generic/sdkconfig
build/core2/sdkconfig
build/stickc_plus2/sdkconfig
```

Common defaults are stored in:

```text
sdkconfig.defaults
```

Board-specific defaults are stored in:

```text
sdkconfig.defaults.generic
sdkconfig.defaults.core2
sdkconfig.defaults.stickc_plus2
```

---

# First Boot

After flashing:

1. Reset or power-cycle the board.
2. Enable Bluetooth on the phone.
3. Find **Satura Bridge**.
4. Pair with the device.
5. If a PIN is requested, use `0000`.
6. Connect the phone to Satura Bridge through Bluetooth PAN.
7. Open the phone browser.
8. Open:

```text
http://192.168.7.1
```

9. Configure the Wi-Fi uplink.

Saved Wi-Fi networks survive reboot.

---

# Serial Monitor

For M5Stack Core2:

```powershell
idf.py -B build\core2 -p COM6 monitor
```

To flash and monitor in one command:

```powershell
idf.py -B build\core2 -p COM6 flash monitor
```

Exit the monitor with:

```text
Ctrl+]
```

---

# Important

Do not use a firmware image from another board profile.

For example:

```text
M5Stack Core2 → core2 firmware
M5StickC Plus2 → stickc_plus2 firmware
Generic ESP32 → generic firmware
```

The profiles use different flash sizes and board-specific hardware initialization.

In particular:

* Generic ESP32 uses a 2 MB flash configuration.
* M5Stack Core2 uses a 16 MB flash configuration.
* M5StickC Plus2 uses an 8 MB flash configuration.

The full images produced by `build.ps1` are already merged and must be flashed at offset `0x0`.
