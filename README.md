# WifiPortal

A simple WiFi configuration library for ESP32.
Provides a **captive-portal–style configuration page** to set the SSID and password via a web browser, when no WiFi credentials are available or the connection fails.

## Features

- Works as Wi-Fi AP and captive portal when WiFi is not configured
- Can set Wi-Fi credentials and key-value strings using Preferences
- Can upload files to LittleFS

## Requirements

- ESP32
- Arduino framework
- ESP32 Arduino core

## Usage

```
#include "WifiPortal.h"
WifiPortal wifiPortal("app");

void setup()
{
    wifiPortal.startConfigPortal({"key", "key2"});
}
```

See examples/basic for details.

## License
MIT License
This library depends on ESP32 Arduino core libraries
licensed under the Apache License 2.0.
