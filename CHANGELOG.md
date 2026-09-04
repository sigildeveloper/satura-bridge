# Changelog

All notable changes to this project will be documented in this file.

## [0.0.16] – 2026-09-04

### Added
- **Clipboard (pasteboard)** – persistent storage for text snippets and links,
  accessible via web UI (`/clip`). Supports pinning favorites (up to 8),
  auto‑detection of HTTP/HTTPS links, and ring buffer (15 items).
  Data survives reboots.
- **Hidden Wi-Fi networks** – you can now add and connect to SSIDs that do not
  broadcast their name. A checkbox in the “Add network” form marks a network as
  hidden.
- **Device name configuration** – change the Bluetooth device name via web UI
  (`/name`); persists across reboots and auto‑generates a unique default
  (e.g., “Satura Bridge A1B2”) using the last bytes of the BT MAC.
- **Better Bluetooth re‑opening** – after disconnection (HCI or BNEP), the
  bridge becomes visible again automatically, with a delay to avoid busy loops.

### Changed
- **Web UI overhaul**:
  - New “Manage Networks” page (`/networks`) – shows scanned APs, saved
    networks, allows adding/removing, and supports hidden SSIDs.
  - Status page now shows battery percentage (if board supports it) and proxy
    enabled/disabled.
  - Proxy configuration page (`/proxy`) with enable/disable and host/port.
  - Device name page (`/name`).
  - Clipboard page (`/clip`).
- **Board support** – board abstraction (`board/`) now provides battery and
  charging status via `board_get_battery_percent()`, etc.
- **Logging** – more informative heartbeat logs, including httpd stack headroom.
- **Default Bluetooth name** – now derived from MAC address (instead of fixed
  “Satura Bridge”).

### Fixed
- **Stability**:
  - Fixed potential crashes when `nvs_flash_init()` fails on first boot (now
    erases and retries).
  - Fixed buffer overflow in `nvs_storage_load_networks()` when caller supplies
    a smaller buffer than stored count (now reads into a full‑size local array
    first).
  - Fixed BNEP channel close not making the device visible again if HCI
    disconnected before BNEP opened.
  - Fixed DNS spoofing vulnerability – reply is accepted only from the same
    upstream IP:port that was queried.
  - Fixed proxy relay blocking indefinitely when upstream keeps connection open
    (now respects Content‑Length and uses timeouts).
  - Fixed `httpd` stack overflow risk by increasing stack size to 12 KB and
    monitoring headroom.
- **Wi‑Fi**:
  - Connecting to a network with empty password now uses WIFI_AUTH_OPEN.
  - Retry mechanism resets when credentials change.
  - Hidden networks are now attempted even if not visible in scan results.
- **Bluetooth**:
  - PIN code request now responds with “0000” (legacy pairing).
  - SSP user confirmation is auto‑accepted.
  - RSSI value 0 (from unready connection) is ignored to avoid bogus reading.
- **HTTP**:
  - POST requests to `/setup` and `/networks/add` now correctly handle hidden
    flag.
  - `handler_name_post` rejects empty or too‑long names without rebooting.
  - Port number validation in proxy settings rejects non‑numeric input.
- **Build** – CMake now selects the correct board source file based on Kconfig.

### Security
- DNS upstream source validation prevents trivial spoofing on shared networks.
- Proxy gateway forwards only essential headers (User‑Agent, Cookie, etc.) and
  drops hop‑by‑hop headers.

### Deprecated
- None.

### Removed
- None.

### Known Issues
- Windows 10 Internet Connection Sharing via BT PAN is not supported.
- M5Stack Core2 v1.1 (with AXP2101) is not supported fully.

## [Unreleased]

### Added

* Board abstraction layer under `main/board/`
* Generic ESP32 DevKit board profile
* M5Stack Core2 board profile
* M5StickC Plus2 board profile
* Independent ESP-IDF build configurations for each supported board
* Board-specific `sdkconfig.defaults` files
* Unified PowerShell build script
* Automatic merged full firmware generation for each board
* Board-specific firmware output directories

### Changed

* Build system now supports multiple hardware targets from the same source tree
* Board-specific hardware code is isolated from application and networking code
* Battery handling now uses the board abstraction layer
* Flash size is selected per hardware profile
* `menuconfig` is now independent for each board profile
* Firmware output is organized by board

### Build profiles

* `generic` — Generic ESP32 DevKit, 2 MB flash
* `core2` — M5Stack Core2 original revision with AXP192, 16 MB flash
* `plus2` — M5StickC Plus2, 8 MB flash

### Documentation

* Updated README with supported hardware and board-specific build instructions
* Updated flashing documentation for all supported boards
* Documented independent board configurations and menuconfig usage

---

## [0.0.15] - 2026-08-26

### Added

* Transport-agnostic link abstraction (`link_iface.h` / `link_registry.c`):
  `nat_bridge` and future code reference `LINK_ROLE_DOWNLINK`/`LINK_ROLE_UPLINK`
  instead of BT PAN or WiFi directly — lays groundwork for BT PPP, serial/IR
  downlinks, and Ethernet/modem uplinks without touching `nat_bridge.c` again
* `EVENT_DOWNLINK_UP/DOWN` and `EVENT_UPLINK_UP/DOWN` generic events on the
  event bus, published by `bt_pan` and `wifi_manager` alongside their
  existing BT/WiFi-specific events
* httpd worker task stack high-water-mark logging in the heartbeat, for
  empirical stack-margin monitoring instead of static estimates

### Changed

* `bt_pan` no longer calls into `wifi_manager` directly — reacts to
  `EVENT_BT_CONNECTED` instead, removing the module's WiFi dependency
* Split `http_server.c` into `http_routes.c` (UI/config pages) and
  `proxy_relay.c` (raw-socket proxy relay engine); `http_server.c` is now
  just startup + URI table
* `proxy_relay`'s response framing to the client rewritten from HTTP/1.1
  chunked transfer to raw HTTP/1.0 with `Content-Length` passthrough (or
  `Connection: close`) — chunked encoding was resetting connections on
  older/retro browsers that predate it
* Removed the legacy single-network `nvs_storage_save/load/clear` API,
  unused since the multi-network migration
* `pan_wifi_bridge.c` renamed to `app_bootstrap.c`, stale version-9
  changelog comment in its header replaced with a description of what it
  actually does now
* Pinned `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` (`-Os`) explicitly — cuts
  IRAM usage from 79.7% to 74.31% and shrinks the image by ~65 KB;
  previously relied on the ESP-IDF project template default, which is
  `-Og` and leaves noticeably less IRAM headroom for the BT+WiFi
  combination

### Fixed

* DNS forwarder (`dns_server.c`) accepted a spoofed upstream reply from
  any source on the local network as long as the 16-bit transaction ID
  matched — now also verifies the reply's source IP and port match the
  configured upstream DNS server
* `event_bus.c` had `EVENT_TYPE_COUNT` hardcoded at 4; any event type
  added past that silently failed to register or fire

### Repo hygiene

* Untracked `components/btstack-esp32/build/` artifacts that had been
  committed before `build/` was added to `.gitignore`, causing constant
  local diff noise on every rebuild

### Tested

* Sony Ericsson J108 — BT PAN connect/reconnect, WiFi failover, NAT
  enable/disable on independent link state changes, proxy relay against
  nnproject.cc (HTML + CSS + image, no errors)

---

## [0.0.14] - 2026-08-13

### Architecture

* Rewrote the WiFi manager as a single-owner state machine with an
  event queue — one worker task now owns all connect/scan/retry/
  recovery decisions, eliminating the guard-flag races from earlier
  versions
* Added a lightweight event bus (`event_bus.c`) — WiFi/BT connection
  state changes now publish events instead of using direct callback
  wiring between modules
* Centralized all NVS access behind a `storage.c` abstraction
* Extracted the watchdog task into its own module

---

## [0.0.13] - 2026-08-13

### HTTP Proxy Improvements

Satura Bridge now supports HTTP POST requests through the configured proxy and forwards selected response headers to the client.

### What's new

* HTTP POST request support through the proxy
* POST body forwarding
* Cookie forwarding
* Response header forwarding
* Improved compatibility with legacy browsers and applications that use HTTP proxy access

### Compatibility & Testing

* Tested HTTP connectivity with legacy mobile software
* Existing Wi-Fi network management and HTTP proxy functionality are retained

### Firmware

* Added prebuilt `satura-bridge-v0.0.13.bin`
* Updated firmware and documentation version references

---

## [0.0.12] - 2026-08-13

### HTTP Proxy Gateway

Satura Bridge now supports an HTTP proxy gateway for legacy devices and WAP-style proxy services.

### What's new

* HTTP proxy gateway support
* New `/proxy` page in the web interface for proxy configuration
* Proxy settings are stored and used by the bridge
* Added a dedicated `proxy_gateway` module to keep proxy functions separate from the main bridge code
* Support for legacy HTTP proxy gateways, including WAP compression proxy use cases
* Improved compatibility with legacy browsers and applications that use HTTP proxy access

### Compatibility & Testing

* Tested HTTP connectivity with legacy mobile software
* Existing multi-network Wi-Fi management from v0.0.11 is retained

### Firmware

* Added prebuilt `satura-bridge-v0.0.12.bin`
* Updated firmware and documentation version references

---

## [0.0.11] - 2026-08-07

### Multiple Wi-Fi Networks

Satura Bridge can now store and manage multiple Wi-Fi networks. It no longer depends on one saved network.

### What's new

* Up to 6 saved Wi-Fi networks
* Wi-Fi network scanning from the web interface
* Automatic network selection based on visible networks and signal strength (RSSI)
* Automatic fallback — if Satura Bridge cannot connect to the preferred network, it tries the next available saved network
* New `/networks` page for saved network management
* `/setup` remains available for manual Wi-Fi configuration
* `/reset` now removes all saved Wi-Fi networks

### Reliability & Fixes

* Fixed a possible stack buffer overflow when saved networks were loaded from NVS
* Increased the HTTP server URI handler limit for the expanded web interface
* Added URL encoding/decoding and HTML escaping for Wi-Fi SSIDs
* Improved the connection flow and status reporting when Satura Bridge tries multiple networks

### Firmware

* Added prebuilt `satura-bridge-v0.0.11.bin`
* Updated firmware and documentation version references

---

## [0.0.10] - 2026-08-06

### Internal Refactor

The `pan_wifi_bridge.c` file was split into separate modules:

* `dns_server`
* `nvs_storage`
* `http_utils`
* `app_state`
* `wifi_manager`
* `bt_pan`
* `http_server`
* `uptime`
* `nat_bridge`

This change did not change the device functions. The existing behavior was verified on real hardware with a Sony Ericsson J108.

---

## [0.0.9] - 2026-07-24

### Added

* Bluetooth Classic PAN / NAP support
* Wi-Fi Internet connectivity
* NAT / routing
* Web-based configuration interface
* Captive portal-style setup
* Automatic Wi-Fi connection
* Wi-Fi connection recovery
* Watchdog and recovery mechanisms
* RSSI and device status information
* Single-file firmware flashing

### Tested

* Sony Ericsson J108
* Older Android devices
* NetFront 3.4
* J2ME applications

### Known limitations

* Only one Bluetooth PAN client can connect at one time
* Windows 10 Internet connectivity is not supported
* Compatibility can vary between devices
