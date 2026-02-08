#include <Arduino.h>
#include <vector>

// Usage:
// WifiPortal portal("app");
// portal.startConfigPortal({"key1", "key2"});
// portal.startWebServer();
// In loop(): portal.handleWebServer();
// To stop: portal.stopWebServer();

//bool tryConnectWiFi(unsigned long timeout_ms);

class WifiPortal
{
public:
    WifiPortal(String prefName, String apSsid = "", String apPass = "12345678");
    ~WifiPortal();
    bool tryConnectWiFi(unsigned long timeout_ms = 15000);
    void startConfigPortal(std::vector<String> keys);
    void startWebServer();
    void handleWebServer();
    void stopWebServer();

    String wifiInfo();
    String ipInfo();

private:
    String _prefName;
    String _apSsid;
    String _apPass;
    std::vector<String> _keys;
    std::vector<String> _networks;
    bool _uploadDone = false;
    bool _serverStarted = false;

    String generateSsidOptions();
    String generateInputList(std::vector<String> keys);
    String generateFileListHTML();

    void handleNotFound();
    void handleRoot();
    void handleSubmit();
    void handleUpload();
    void handleDeleteFile();
};
