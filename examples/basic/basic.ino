#include <Arduino.h>
#include <Preferences.h>
#include "WifiPortal.h"

WifiPortal wifiPortal("app");
// By default, the portal SSID will be "ESP32-XX" and password "12345678"
// "app" is the namespace for Preferences. You can change it as needed.

void setup()
{
    Serial.begin(115200);
    Serial.println("Start WiFi Portal");

    wifiPortal.startConfigPortal({"test_string", "test_string2"});
    // This will test if saved Wi-Fi credentials exist and connect to Wi-Fi
    // If connection fails, it will start an AP with given SSID and password in loop
    // Users can connect to the AP and configure Wi-Fi credentials via web portal
    // After saving credentials, ESP32 will reboot to connect with Wi-Fi

    Serial.println("Wi-Fi connected");

    Preferences prefs;
    prefs.begin("app", true);
    String wifi_ssid = prefs.getString("wifi_ssid", "none");
    String wifi_pass = prefs.getString("wifi_pass", "none");
    String test_string = prefs.getString("test_string", "default_value");
    String test_string2 = prefs.getString("test_string2", "default_value2");
    prefs.end();
    Serial.printf("SSID: %s\n", wifi_ssid.c_str());
    Serial.printf("PASS: %s\n", wifi_pass.c_str());
    Serial.printf("test_string: %s\n", test_string.c_str());
    Serial.printf("test_string2: %s\n", test_string2.c_str());
    // The above retrieves custom preferences saved via the web portal

    wifiPortal.startWebServer();
    // Start the web server to handle further configuration if needed
}

void loop()
{
    wifiPortal.handleWebServer();
    // Handle web server in the main loop
}