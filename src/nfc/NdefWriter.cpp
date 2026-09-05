#include "NdefWriter.h"
#include "NdefMessageBuilder.h"

#include <cstring>

NdefWriter::NdefWriter(Pn532Client& pn532)
    : pn532_(pn532)
{
}

bool NdefWriter::writeText(const String& text)
{
    return writeNdefPayload(NdefMessageBuilder::textRecord(text));
}

bool NdefWriter::writeUrl(const String& url)
{
    return writeNdefPayload(NdefMessageBuilder::urlRecord(url));
}

String NdefWriter::lastError() const
{
    return lastError_;
}

bool NdefWriter::writeNdefPayload(const std::vector<uint8_t>& payload)
{
    if (payload.size() > 130) {
        setError("NDEF too large");
        return false;
    }

    std::vector<uint8_t> tlv;
    tlv.push_back(0x03);
    tlv.push_back(payload.size());
    tlv.insert(tlv.end(), payload.begin(), payload.end());
    tlv.push_back(0xFE);

    while (tlv.size() % 4 != 0) {
        tlv.push_back(0x00);
    }

    uint8_t pageData[4];
    uint8_t page = 4;

    for (size_t i = 0; i < tlv.size(); i += 4) {
        memcpy(pageData, &tlv[i], 4);

        if (!pn532_.ntagWritePage(page, pageData)) {
            setError(pn532_.lastError());
            return false;
        }

        page++;
    }

    return true;
}

void NdefWriter::setError(const String& error)
{
    lastError_ = error;
}
