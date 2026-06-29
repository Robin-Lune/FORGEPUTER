#pragma once

#include <Arduino.h>
#include <array>
#include <vector>

class KeyStore {
public:
    bool begin();
    const std::vector<std::array<uint8_t, 6>>& keys() const;
    String status() const;

private:
    std::vector<std::array<uint8_t, 6>> keys_;
    String status_ = "Not loaded";

    void addDefaultKeys();
    void loadFile(const char* path);
    bool addKeyLine(const String& line);
    void ensureKeyFiles();
    bool shouldRefreshSystemKeys();
    void writeSystemKeys();
};
