#pragma once

#include "../pn532/Pn532Client.h"

#include <vector>

class NdefWriter {
public:
    explicit NdefWriter(Pn532Client& pn532);

    bool writeText(const String& text);
    bool writeUrl(const String& url);
    String lastError() const;

private:
    Pn532Client& pn532_;
    String lastError_;

    bool writeNdefPayload(const std::vector<uint8_t>& payload);
    void setError(const String& error);
};
