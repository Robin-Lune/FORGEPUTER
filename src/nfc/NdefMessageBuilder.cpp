#include "NdefMessageBuilder.h"

std::vector<uint8_t> NdefMessageBuilder::textRecord(const String& text)
{
    std::vector<uint8_t> record;
    const String lang = "en";
    const uint8_t payloadLength = text.length() + 1 + lang.length();

    record.push_back(0xD1);
    record.push_back(0x01);
    record.push_back(payloadLength);
    record.push_back('T');
    record.push_back(lang.length());
    record.push_back('e');
    record.push_back('n');

    for (size_t i = 0; i < text.length(); i++) {
        record.push_back(text[i]);
    }

    return record;
}

std::vector<uint8_t> NdefMessageBuilder::urlRecord(const String& rawUrl)
{
    String url = rawUrl;
    uint8_t prefix = 0x00;

    if (url.startsWith("https://www.")) {
        prefix = 0x02;
        url.remove(0, 12);
    } else if (url.startsWith("https://")) {
        prefix = 0x04;
        url.remove(0, 8);
    } else if (url.startsWith("http://www.")) {
        prefix = 0x01;
        url.remove(0, 11);
    } else if (url.startsWith("http://")) {
        prefix = 0x03;
        url.remove(0, 7);
    }

    std::vector<uint8_t> record;
    record.push_back(0xD1);
    record.push_back(0x01);
    record.push_back(url.length() + 1);
    record.push_back('U');
    record.push_back(prefix);

    for (size_t i = 0; i < url.length(); i++) {
        record.push_back(url[i]);
    }

    return record;
}
