#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

enum NetMode {
    NET_STA,
    NET_AP,
    NET_OFFLINE
};

class WifiManager {
public:
    void begin(NetMode mode = NET_STA);
    void update();
    bool isConnected();
    IPAddress getIP();
    String getSSID();
    NetMode getMode();
private:
    NetMode _mode = NET_OFFLINE;
    uint8_t _reconnectCount = 0;
    void _startSTA();
    void _startAP();
};

extern WifiManager g_wifiManager;

#endif
