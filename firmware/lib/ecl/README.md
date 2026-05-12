
# ECL - Embedded Configuration Library

ECL is a header-only, macro-driven framework for ESP8266/ESP32 and Arduino. It eliminates boilerplate code for WiFi, MQTT, OTA updates, Telnet logging, and SoftwareSerial. 

By defining configuration flags **before** including the library, only the required code is compiled, ensuring zero memory overhead for unused features.

This README is defenetly not fully AI generated, I swear...

## Features

* **Zero-Boilerplate Setup:** Initialize WiFi, OTA, MQTT, and more with just `ECL::begin()`.
* ** MQTT Routing:** Supports dynamic callbacks and topic wildcards (`+`, `#`). Automatically handles reconnections and re-subscriptions.
* **Unified Logging:** `ECL::log()` and `ECL::logf()` print to both Hardware Serial and a live Telnet server simultaneously.
* **Event-Driven Hardware:** Includes an asynchronous `Button` module with debouncing, press, release, and long-press events.
* **Auto-Looping:** Hardware modules (like `Button`) automatically register to `ECL::loop()`, keeping your main `loop()` completely clean.

## Quick Start

### [Sample code](./examples/example.cpp)

## Configuration Flags

Define these in your main `cpp` or `ino` file **before** `#include <ECL.h>` to enable specific features. If a feature's primary macro is missing, that feature's code will not be compiled, saving memory.

| Feature | Macro Definition | Default Value (if omitted) |
| :--- | :--- | :--- |
| **Hardware Serial** | `ECL_SERIAL_SPEED` | *Disabled* |
| **WiFi** | `ECL_WIFI_SSID`<br>`ECL_WIFI_PASSWORD` | *Disabled*<br>`"iotempire"` |
| **Software Serial** | `ECL_SOFTWARE_SERIAL_RX`<br>`ECL_SOFTWARE_SERIAL_TX`<br>`ECL_SOFTWARE_SERIAL_SPEED`| *Disabled*<br>*Disabled*<br>`9600` |
| **Telnet Logs** | `ECL_TELNET_PORT` | *Disabled* |
| **MQTT** | `ECL_MQTT_SERVER`<br>`ECL_MQTT_PORT`<br>`ECL_MQTT_CLIENT` | *Disabled*<br>`1883`<br>`"node"` |
| **OTA Updates** | `ECL_OTA_HOSTNAME`<br>`ECL_OTA_PASSWORD` | *Disabled*<br>`"iotempower"` |

> **Note:** Enabling **OTA**, **MQTT**, or **Telnet** automatically requires a WiFi connection, so `ECL_WIFI_SSID` must also be defined.

---

## API Reference

### Core Library (`ECL.h`)

* **`ECL::begin()`**
  Initializes all enabled modules (Serial, WiFi, OTA, MQTT, Telnet). Call this once inside `setup()`.
* **`ECL::loop()`**
  Maintains background tasks like handling OTA updates, MQTT reconnections/polling, and registered hardware callbacks. Call this continuously inside `loop()`.
* **`ECL::log(String msg)`** / **`ECL::logf(const char *format, ...)`**
  Prints formatted messages to Hardware Serial and mirrors them to the Telnet client (if connected).
* **`ECL::mqttPublish(const char *topic, const char *payload)`**
  Publishes a message to the configured MQTT broker.
* **`ECL::mqttSubscribe(const char *topic, callback)`**
  Registers a callback for an incoming MQTT topic. Supports dynamic routing and standard wildcards (`+`, `#`). The callback can be in two formats:
  * `(char *topic, byte *payload, unsigned int length)`
  * `(char *topic, char *payload)`
* **`ECL::addToLoop(std::function<void()> cb)`**
  Registers a custom background function to be executed every time `ECL::loop()` runs.

---

### Hardware Modules (`Button.h`)

* **`ECL::Button(uint8_t pin)`**
  Initializes a new button instance on the specified GPIO pin (uses `INPUT_PULLUP`).
* **`setOnPress(callback)`**
  Executes the callback when the button is pushed down.
* **`setOnRelease(callback)`**
  Executes the callback when the button is released.
* **`setOnLongPress(callback)`**
  Executes the callback if the button is held down continuously (default 1000ms).
* **`isPressed()`**
  Returns `true` if the button is currently held down.

> **Note:** Once initialized via any setter or getter, the Button automatically registers itself to `ECL::loop()` to handle debouncing and state changes seamlessly in the background.