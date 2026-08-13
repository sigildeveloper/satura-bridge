# Changelog

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

## [0.0.11] - 2026-08-07

### Multiple Wi-Fi Networks

Satura Bridge can now store and manage multiple Wi-Fi networks. It no longer depends on one saved network.

### What's new

* **Up to 6 saved Wi-Fi networks**
* **Wi-Fi network scanning** from the web interface
* **Automatic network selection** based on visible networks and signal strength (RSSI)
* **Automatic fallback** — if Satura Bridge cannot connect to the preferred network, it tries the next available saved network
* New **`/networks`** page for saved network management

  * Scan for nearby Wi-Fi networks
  * Add networks
  * Connect to a scanned network
  * Remove saved networks
  * View signal strength
* `/setup` remains available for **manual Wi-Fi configuration**
* `/reset` now removes **all saved Wi-Fi networks**

### Reliability & Fixes

* Fixed a possible **stack buffer overflow** when saved networks were loaded from NVS
* Increased the HTTP server URI handler limit for the expanded web interface
* Added URL encoding/decoding and HTML escaping for Wi-Fi SSIDs
* Improved the connection flow and status reporting when Satura Bridge tries multiple networks

### Firmware

* Added prebuilt `satura-bridge-v0.0.11.bin`
* Updated firmware and documentation version references

This release builds on the modular codebase introduced in v0.0.10. It adds a Wi-Fi management system for devices that can move between multiple networks.

## [0.0.12] - 2026-08-13

### HTTP Proxy Gateway

Satura Bridge now supports an HTTP proxy gateway for legacy devices and WAP-style proxy services.

### What's new

* **HTTP proxy gateway support**
* New **`/proxy`** page in the web interface for proxy configuration
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

This release adds the first dedicated proxy function to Satura Bridge. It allows the bridge to use HTTP proxy gateways that are used by some legacy mobile Internet services and applications.

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
