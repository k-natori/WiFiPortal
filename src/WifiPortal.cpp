#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "WifiPortal.h"

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Setup</title>
<style>
  body {
    font-family: sans-serif;
    background: #f5f5f5;
    margin: 0;
    padding: 16px;
    display: flex;
    justify-content: center;
  }
  .card {
    background: white;
    padding: 20px;
    border-radius: 12px;
    max-width: 400px;
    width: 100%;
    box-shadow: 0 2px 6px rgba(0,0,0,0.15);
  }
  h2 { text-align: center; margin-top: 0; }
  label { font-weight: bold; display: block; margin-top: 16px; }
  select, input {
    width: 100%; padding: 10px; font-size: 16px;
    margin-top: 6px; border: 1px solid #ccc;
    border-radius: 6px; box-sizing: border-box;
  }
  button {
    width: 100%; padding: 12px; font-size: 18px;
    margin-top: 24px; background: #4CAF50;
    color: white; border: none; border-radius: 8px;
  }
    .files {
  margin-top: 10px;
}
.file {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px;
  border-bottom: 1px solid #ddd;
}
.file button {
  background: #e74c3c;
  color: white;
  border: none;
  padding: 6px 10px;
  border-radius: 6px;
}
</style>
</head>
<body>
<div class="card">
<h2>ESP32 Setup</h2>
<form action="/save" method="POST">
  <label>Wi-Fi SSID</label>
  <select name="wifi_ssid">
    {{SSID_LIST}}
  </select>

  <label>Wi-Fi Password</label>
  <input type="password" name="wifi_pass" value="{{WIFI_PASS}}">

  {{INPUT_LIST}}

  <button type="submit">Save & Reboot</button>
</form>
{{FILE_LIST}}
<form method="POST" action="/upload" enctype="multipart/form-data">
  <label>Upload files</label>
  <input type="file" name="upload">
  <button type="submit">Upload</button>
</form>
</div>
</body>
</html>
)rawliteral";

const IPAddress myAPIP(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);

// Generate device AP name based on MAC address
String generateDeviceApName()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[16];
    snprintf(buf, sizeof(buf), "ESP32-%02X", mac[5]);
    return String(buf);
}

// Try to connect to WiFi with timeout and up to 3 retries
bool WifiPortal::tryConnectWiFi(unsigned long timeout_ms)
{
    Preferences prefs;
    prefs.begin(_prefName.c_str(), true);
    String wifi_ssid = prefs.getString("wifi_ssid", "");
    String wifi_pass = prefs.getString("wifi_pass", "");
    prefs.end();

    static const int MAX_TRIES = 3;

    delay(500);

    for (int attempt = 1; attempt <= MAX_TRIES; attempt++)
    {
        if (attempt > 1)
        {
            WiFi.disconnect(true);
            delay(500);
        }

        if (wifi_ssid.isEmpty() || wifi_pass.isEmpty())
        {
            Serial.printf("[WiFiPortal] Attempt %d/%d: connecting to open network...\n", attempt, MAX_TRIES);
            WiFi.begin();
        }
        else
        {
            Serial.printf("[WiFiPortal] Attempt %d/%d: connecting to SSID: %s\n", attempt, MAX_TRIES, wifi_ssid.c_str());
            WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        }

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms)
        {
            delay(300);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("\n[WiFiPortal] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.printf("\n[WiFiPortal] Attempt %d/%d timed out after %lu ms\n", attempt, MAX_TRIES, timeout_ms);
    }

    Serial.println("[WiFiPortal] All connection attempts failed.");
    return false;
}

// wifi_portal class implementation
WifiPortal::WifiPortal(String prefName, String apSsid, String apPass)
{
    _prefName = prefName;
    _apSsid = apSsid;
    _apPass = apPass;
    if (_prefName.isEmpty())
    {
        _prefName = "app";
    }
    if (_apSsid.isEmpty())
    {
        _apSsid = generateDeviceApName();
    }
}

// Destructor
WifiPortal::~WifiPortal()
{
    if (_webServer != nullptr) {
        _webServer->stop();
        delete _webServer;
        _webServer = nullptr;
    }
    if (_dnsServer != nullptr) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }
}

// Start configuration portal
void WifiPortal::startConfigPortal(std::vector<String> keys)
{
    _keys = keys;
    // Attempt to connect to previously saved WiFi credentials
    Serial.println("[WiFiPortal] Attempting to connect with saved credentials...");
    WiFi.mode(WIFI_STA);
    if (tryConnectWiFi(15000))
    {
        Serial.println("[WiFiPortal] WiFi connected!");
        Serial.print("[WiFiPortal] IP: ");
        Serial.println(WiFi.localIP());
        return;
    }
    Serial.println("[WiFiPortal] WiFi connection failed. Starting AP mode...");
    WiFi.disconnect();
    delay(100);

    _networks.clear();
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFiPortal] Found %d available networks\n", n);
    for (int i = 0; i < n; i++)
    {
        String ssid = WiFi.SSID(i);
        _networks.push_back(ssid);
    }

    // Start AP mode for configuration
    String apName = _apSsid;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str(), _apPass);
    WiFi.softAPConfig(myAPIP, myAPIP, subnet);

    Serial.printf("[WiFiPortal] AP started: %s / %s\n", apName.c_str(), _apPass);
    Serial.printf("[WiFiPortal] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    _webServer = new WebServer(80);
    _dnsServer = new DNSServer();
    _dnsServer->start(53, "*", WiFi.softAPIP());
    startWebServer();

    while (true)
    {
        _dnsServer->processNextRequest();
        _webServer->handleClient();
    }
}

// Start the web server with given keys
void WifiPortal::startWebServer()
{
    if (_serverStarted)
    {
        Serial.println("[WiFiPortal] WebServer already started.");
        return;
    }

    Serial.println("[WiFiPortal] Starting WebServer...");

    // Scan for networks if not already done
    if (WiFi.getMode() != WIFI_AP && _networks.size() == 0)
    {
        Serial.println("[WiFiPortal] Scanning networks...");
        _networks.clear();
        int n = WiFi.scanNetworks();
        Serial.printf("[WiFiPortal] Found %d networks\n", n);
        for (int i = 0; i < n; i++)
        {
            String ssid = WiFi.SSID(i);
            _networks.push_back(ssid);
        }
    }

    // Redirect all unknown requests to the main page
    _webServer->onNotFound([this]()
                      { handleNotFound(); });

    // Serve the main page
    _webServer->on("/", HTTP_GET, [this]()
              { handleRoot(); });

    // Handle form submission
    _webServer->on("/save", HTTP_POST, [this]()
              { handleSubmit(); });
    // Handle file upload
    _webServer->on("/upload", HTTP_POST, [this]()
              {
        if (_uploadDone) {
            _webServer->sendHeader("Location", "/", true);
            _webServer->send(302, "text/plain", "");
            _uploadDone = false;
        } }, [this]()
              { handleUpload(); });
    // Handle file deletion
    _webServer->on("/delete", HTTP_POST, [this]()
              { handleDeleteFile(); });

    // Favicon request
    _webServer->on("/favicon.ico", HTTP_GET, [this]()
              { _webServer->send(204); });

    _webServer->begin();
    _serverStarted = true;
    Serial.println("[WiFiPortal] WebServer started on port 80");
    Serial.printf("[WiFiPortal] WiFi Mode: %d, SSID: %s, IP: %s\n", WiFi.getMode(), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

// Handle web server in the main loop
void WifiPortal::handleWebServer()
{
    if (_serverStarted)
    {
        _webServer->handleClient();
        // Uncomment for debugging client requests
        // Serial.println("[WiFiPortal] handleWebServer() called");
    }
}

// Stop the web server
void WifiPortal::stopWebServer()
{
    if (_serverStarted)
    {
        _webServer->stop();
        _serverStarted = false;
        Serial.println("[WiFiPortal] WebServer stopped");
    }
}

// Generate WiFi info string
String WifiPortal::wifiInfo()
{
    String info = "WIFI:S:" + _apSsid + ";T:WPA;P:" + _apPass + ";;";
    return info;
}

// Generate IP info string
String WifiPortal::ipInfo()
{
    String info = "http://" + WiFi.localIP().toString();
    return info;
}

// Generate SSID options for HTML select
String WifiPortal::generateSsidOptions()
{
    Preferences prefs;
    prefs.begin(_prefName.c_str(), true);
    String wifi_ssid = prefs.getString("wifi_ssid", "");
    prefs.end();

    int n = _networks.size();
    String options = "";
    for (int i = 0; i < n; i++)
    {
        String ssid = _networks[i];
        if (ssid == wifi_ssid)
        {
            options += "<option value=\"" + ssid + "\" selected>" + ssid + "</option>";
            continue;
        }
        else
        {
            options += "<option value=\"" + ssid + "\">" + ssid + "</option>";
        }
    }
    return options;
}

// Generate input fields for additional keys
String WifiPortal::generateInputList(std::vector<String> keys)
{
    Preferences prefs;
    prefs.begin(_prefName.c_str(), true);

    String inputs = "";
    for (size_t i = 0; i < keys.size(); i++)
    {
        String value = prefs.getString(keys[i].c_str(), "");
        inputs += "<label>" + keys[i] + "</label>\n";
        inputs += "<input name=\"" + keys[i] + "\" value=\"" + value + "\">\n\n";
    }

    prefs.end();
    return inputs;
}

// Generate file list on littleFS
String WifiPortal::generateFileListHTML()
{
    if (!(LittleFS.begin(false)))
    {
        if (!LittleFS.begin(true))
        {
            Serial.println("An Error has occurred while mounting LittleFS");
            return "<p>Error mounting filesystem.</p>";
        }
    }

    String fileListHTML = "<div class=\"files\">\n";

    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    int percent = (total > 0) ? (used * 100 / total) : 0;
    fileListHTML += "<p>Storage: " + String(used) + " / " + String(total) + " bytes (" + String(percent) + "% used)</p>\n";

    File root = LittleFS.open("/");
    File file = root.openNextFile();

    if (!file)
    {
        fileListHTML += "<p>No files found.</p>\n";
    }

    while (file)
    {
        String filename = String(file.name());
        size_t size = file.size();
        fileListHTML += "<div class=\"file\">\n";
        fileListHTML += "<span>" + filename + " (" + String(size) + " bytes)</span>";
        fileListHTML += "<form method=\"POST\" action=\"/delete\">\n";
        fileListHTML += "<input type=\"hidden\" name=\"filename\" value=\"" + filename + "\">\n";
        fileListHTML += "<button type=\"submit\">Delete</button>\n";
        fileListHTML += "</form>\n";
        fileListHTML += "</div>\n";
        file = root.openNextFile();
    }
    fileListHTML += "</div>\n";

    return fileListHTML;
}

// Handle 404 - Redirect to root
void WifiPortal::handleNotFound()
{
    Serial.printf("[WiFiPortal] 404 %s - Client: %s\n", _webServer->uri().c_str(), _webServer->client().remoteIP().toString().c_str());
    String urlString = "/";
    _webServer->sendHeader("Location", urlString);
    _webServer->send(302, "text/plain", "Found.");
}

// Serve the main configuration page
void WifiPortal::handleRoot()
{
    Serial.printf("[WiFiPortal] GET / - Client: %s\n", _webServer->client().remoteIP().toString().c_str());
    String page = INDEX_HTML;
    Preferences prefs;
    prefs.begin(_prefName.c_str(), true);
    String wifi_pass = prefs.getString("wifi_pass", "");
    prefs.end();

    page.replace("{{SSID_LIST}}", generateSsidOptions());
    page.replace("{{WIFI_PASS}}", wifi_pass);
    page.replace("{{INPUT_LIST}}", generateInputList(_keys));
    page.replace("{{FILE_LIST}}", generateFileListHTML());

    _webServer->send(200, "text/html", page);
}

// Handle form submission and save preferences
void WifiPortal::handleSubmit()
{
    Serial.println("Form submitted.");
    String wifi_ssid = _webServer->arg("wifi_ssid");
    String wifi_pass = _webServer->arg("wifi_pass");

    Preferences prefs;
    prefs.begin(_prefName.c_str(), false);
    prefs.putString("wifi_ssid", wifi_ssid.c_str());
    prefs.putString("wifi_pass", wifi_pass.c_str());
    for (size_t i = 0; i < _keys.size(); i++)
    {
        String key = _keys[i];
        prefs.putString(key.c_str(), _webServer->arg(key));
    }
    prefs.end();

    _webServer->send(200, "text/plain", "Saved! Rebooting...");
    _webServer->stop();
    _dnsServer->stop();

    // Reconnect to WiFi with new credentials
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to SSID: %s, pass: %s\n", wifi_ssid.c_str(), wifi_pass.c_str());
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        delay(300);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nFailed to connect to WiFi.");
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);
    ESP.restart();
}

// Handle file upload
void WifiPortal::handleUpload()
{
    static File fsUploadFile;
    HTTPUpload &upload = _webServer->upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("UploadStart: %s\n", upload.filename.c_str());
        if (!(LittleFS.begin(false)))
        {
            if (!LittleFS.begin(true))
            {
                Serial.println("An Error has occurred while mounting LittleFS");
                return;
            }
        }
        fsUploadFile = LittleFS.open("/" + upload.filename, "w");
        if (!fsUploadFile)
        {
            Serial.println("Failed to open file for writing");
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        Serial.printf("UploadWrite: %s, Size: %d\n", upload.filename.c_str(), upload.currentSize);
        if (fsUploadFile)
        {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        _uploadDone = true;
        Serial.printf("UploadEnd: %s, Size: %d\n", upload.filename.c_str(), upload.totalSize);
        if (fsUploadFile)
        {
            fsUploadFile.close();
        }
    }
}

// Handle file deletion
void WifiPortal::handleDeleteFile()
{
    if (!(LittleFS.begin(false)))
    {
        if (!LittleFS.begin(true))
        {
            Serial.println("An Error has occurred while mounting LittleFS");
            return;
        }
    }
    String filename = _webServer->arg("filename");
    if (LittleFS.exists("/" + filename))
    {
        LittleFS.remove("/" + filename);
        Serial.printf("Deleted file: %s\n", filename.c_str());
    }
    _webServer->sendHeader("Location", "/", true);
    _webServer->send(302, "text/plain", "");
}