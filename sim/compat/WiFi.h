// Shim WiFi pour la simulation desktop. Sans effet.
#pragma once

enum WiFiMode_t {
    WIFI_OFF = 0,
    WIFI_STA = 1,
    WIFI_AP = 2,
    WIFI_AP_STA = 3,
};

class WiFiSim {
public:
    bool disconnect(bool wifioff = false)
    {
        (void)wifioff;
        return true;
    }

    bool mode(WiFiMode_t m)
    {
        (void)m;
        return true;
    }
};

extern WiFiSim WiFi;
