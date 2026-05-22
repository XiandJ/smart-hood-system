#include "wifi_manager.h"
#include <WiFi.h>
#include "config.h"

WifiManager g_wifiManager;

void WifiManager::begin(NetMode mode) {
    _mode = mode;
    WiFi.disconnect(true);
    delay(100);
    if (mode == NET_STA) {
        _startSTA();
    } else if (mode == NET_AP) {
        _startAP();
    }
}

void WifiManager::update() {
    if (_mode == NET_STA) {
        if (WiFi.status() != WL_CONNECTED) {
            _reconnectCount++;
            if (_reconnectCount > 10) {
                Serial.println("[WiFi] STA连接失败，切换到AP模式");
                _startAP();
                _mode = NET_AP;
            }
        } else {
            _reconnectCount = 0;
        }
    }
}

bool WifiManager::isConnected() {
    if (_mode == NET_STA) return WiFi.status() == WL_CONNECTED;
    if (_mode == NET_AP)  return true;
    return false;
}

IPAddress WifiManager::getIP() {
    if (_mode == NET_STA) return WiFi.localIP();
    return WiFi.softAPIP();
}

String WifiManager::getSSID() {
    if (_mode == NET_STA) return WIFI_SSID;
    return WIFI_AP_SSID;
}

NetMode WifiManager::getMode() { return _mode; }

void WifiManager::_startSTA() {
    Serial.printf("[WiFi] 连接 STA: %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] STA已连接, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] STA连接超时");
    }
}

void WifiManager::_startAP() {
    Serial.printf("[WiFi] 启动 AP: %s\n", WIFI_AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    Serial.printf("[WiFi] AP已启动, IP: %s\n", WiFi.softAPIP().toString().c_str());
}
