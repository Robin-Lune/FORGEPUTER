// Shim Preferences (NVS ESP32) pour la simulation desktop.
// Stockage en mémoire : les réglages ne survivent pas à la fermeture.
#pragma once

#include <Arduino.h>

#include <map>
#include <string>

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false)
    {
        (void)name;
        (void)readOnly;
        return true;
    }

    void end() {}

    int getInt(const char* key, int defaultValue = 0)
    {
        const auto it = store().find(key);
        return it == store().end() ? defaultValue : it->second;
    }

    size_t putInt(const char* key, int value)
    {
        store()[key] = value;
        return sizeof(int);
    }

    bool isKey(const char* key) { return store().count(key) > 0; }
    bool clear()
    {
        store().clear();
        return true;
    }

private:
    static std::map<std::string, int>& store()
    {
        static std::map<std::string, int> values;
        return values;
    }
};
