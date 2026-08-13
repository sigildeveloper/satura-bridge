# Changelog

## [0.0.9] - 2026-07-24

### Added
- Bluetooth Classic PAN / NAP support
- Wi-Fi Internet connectivity
- NAT / routing
- Web-based configuration interface
- Captive portal-style setup
- Automatic Wi-Fi connection
- Wi-Fi connection recovery
- Watchdog and recovery mechanisms
- RSSI and device status information
- Single-file firmware flashing

### Tested
- Sony Ericsson J108
- Older Android devices
- NetFront 3.4
- J2ME applications

### Known limitations
- One Bluetooth PAN client at a time
- Windows 10 Internet connectivity is not supported
- Compatibility with individual devices may vary

## [0.0.10] - 2026-08-06

Internal refactor: split the monolithic pan_wifi_bridge.c
into focused modules - dns_server, nvs_storage, http_utils, app_state,
wifi_manager, bt_pan, http_server, uptime, nat_bridge. No functional
changes; existing behavior verified on real hardware (Sony Ericsson J108).

## [0.0.11] - 2026-08-07

### Multiple Wi-Fi Networks

Satura Bridge can now store and manage multiple Wi-Fi networks instead of relying on a single saved network.

### What's new

* **Up to 6 saved Wi-Fi networks**
* **Wi-Fi network scanning** directly from the web interface
* **Automatic network selection** based on visible networks and signal strength (RSSI)
* **Automatic fallback** — if the preferred network cannot be connected to, Satura Bridge tries the next available saved network
* New **`/networks`** page for managing saved networks

  * Scan for nearby Wi-Fi networks
  * Add networks
  * Connect to a scanned network
  * Remove saved networks
  * View signal strength
* `/setup` remains available for **manual Wi-Fi configuration**
* `/reset` now clears **all saved Wi-Fi networks**

### Reliability & Fixes

* Fixed a potential **stack buffer overflow** when loading saved networks from NVS
* Increased the HTTP server URI handler limit to support the expanded web interface
* Added URL encoding/decoding and HTML escaping for Wi-Fi SSIDs
* Improved connection flow and status reporting when trying multiple networks

### Firmware

* Added prebuilt `satura-bridge-v0.0.11.bin`
* Updated firmware and documentation version references

This release builds on the modular codebase introduced in v0.0.10 and adds a more practical Wi-Fi management system for devices that may move between multiple networks.

## [0.0.12] - 2026-08-13

### HTTP Proxy Gateway

Satura Bridge now supports an HTTP proxy gateway for legacy devices and WAP-style proxy services.

### What's new

* **HTTP proxy gateway support**
* New **`/proxy`** page in the web interface for proxy configuration
* Proxy settings are stored and applied by the bridge
* Added a dedicated `proxy_gateway` module to keep proxy functionality separate from the main bridge logic
* Support for legacy HTTP proxy gateways, including WAP compression proxy use cases
* Improved compatibility with legacy browsers and applications that rely on HTTP proxy access

### Compatibility & Testing

* Tested HTTP connectivity with legacy mobile software
* Existing multi-network Wi-Fi management from v0.0.11 is retained

### Firmware

* Added prebuilt `satura-bridge-v0.0.12.bin`
* Updated firmware and documentation version references

This release adds the first dedicated proxy functionality to Satura Bridge, allowing the bridge to work with HTTP proxy gateways used by some legacy mobile Internet services and applications.

