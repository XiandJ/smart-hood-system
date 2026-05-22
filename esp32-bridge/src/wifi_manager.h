#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

enum WifiMode {
    WIFI_MODE_STA,      // 连接已有路由器
    WIFI_MODE_AP,       // 自建热点
    WIFI_MODE_OFFLINE
};

class WifiManager {
public:
    void begin(WifiMode mode = WIFI_MODE_STA) {
        _mode = mode;
        WiFi.disconnect(true);
        delay(100);

        if (mode == WIFI_MODE_STA) {
            _startSTA();
        } else if (mode == WIFI_MODE_AP) {
            _startAP();
        }
    }

    // 检查连接状态，断线自动重连
    void update() {
        if (_mode == WIFI_MODE_STA) {
            if (WiFi.status() != WL_CONNECTED) {
                _reconnectCount++;
                if (_reconnectCount > 10) {  // 连续失败10次，切换到AP模式
                    Serial.println("[WiFi] STA连接失败，切换到AP模式");
                    _startAP();
                    _mode = WIFI_MODE_AP;
                }
            } else {
                _reconnectCount = 0;
            }
        }
    }

    bool isConnected() {
        if (_mode == WIFI_MODE_STA) return WiFi.status() == WL_CONNECTED;
        if (_mode == WIFI_MODE_AP)  return true;
        return false;
    }

    IPAddress getIP() {
        if (_mode == WIFI_MODE_STA) return WiFi.localIP();
        return WiFi.softAPIP();
    }

    String getSSID() {
        if (_mode == WIFI_MODE_STA) return WIFI_SSID;
        return WIFI_AP_SSID;
    }

    WifiMode getMode() { return _mode; }

private:
    WifiMode _mode = WIFI_MODE_OFFLINE;
    uint8_t  _reconnectCount = 0;

    void _startSTA() {
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

    void _startAP() {
        Serial.printf("[WiFi] 启动 AP: %s\n", WIFI_AP_SSID);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
        Serial.printf("[WiFi] AP已启动, IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
};

extern WifiManager g_wifiManager;

#endif
