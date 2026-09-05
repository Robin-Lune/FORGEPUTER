// Pn532BleClient - NDEF tag emulation: CC/NDEF files and APDU responder.

#include "Pn532BleClient.h"

#include <cstring>

bool Pn532BleClient::startNdefEmulation(const NfcDump& dump)
{
    std::vector<uint8_t> ndef;

    if (!extractType2Ndef(dump, ndef)) {
        setError("No NDEF in dump");
        return false;
    }

    return startNdefEmulation(ndef, dump.card);
}

bool Pn532BleClient::startNdefEmulation(const std::vector<uint8_t>& ndefRecord, const CardInfo& card)
{
    if (!connected_) {
        setError("BLE not connected");
        return false;
    }

    if (ndefRecord.empty()) {
        setError("Empty NDEF");
        return false;
    }

    if (ndefRecord.size() > 0xFFFE) {
        setError("NDEF too large");
        return false;
    }

    emulatedNdefFile_.clear();
    emulatedNdefFile_.push_back((ndefRecord.size() >> 8) & 0xFF);
    emulatedNdefFile_.push_back(ndefRecord.size() & 0xFF);
    emulatedNdefFile_.insert(emulatedNdefFile_.end(), ndefRecord.begin(), ndefRecord.end());

    const uint16_t ndefFileSize = static_cast<uint16_t>(emulatedNdefFile_.size());
    const uint16_t maxNdefSize = ndefFileSize > 0x00FF ? ndefFileSize : 0x00FF;
    emulatedCcFile_ = {
        0x00, 0x0F,
        0x20,
        0x00, 0xFF,
        0x00, 0xFF,
        0x04, 0x06,
        0xE1, 0x04,
        static_cast<uint8_t>((maxNdefSize >> 8) & 0xFF),
        static_cast<uint8_t>(maxNdefSize & 0xFF),
        0x00,
        0xFF,
    };

    selectedEmuFile_ = 0;
    emulationExchangeCount_ = 0;
    ble_->setNormalMode();

    NfcDump targetDump;
    targetDump.card = card;
    std::vector<uint8_t> initResponse = ble_->tgInitAsTarget(buildTargetParameters(targetDump));

    if (initResponse.empty()) {
        setError("Emu init timeout");
        return false;
    }

    emulating_ = true;
    return true;
}

void Pn532BleClient::stopNdefEmulation()
{
    emulating_ = false;
    selectedEmuFile_ = 0;

    if (connected_) {
        ble_->inRelease();
        ble_->setNormalMode();
    }
}

bool Pn532BleClient::tickNdefEmulation()
{
    if (!connected_ || !emulating_) {
        return false;
    }

    std::vector<uint8_t> apdu = ble_->getData();

    if (apdu.empty()) {
        ble_->inRelease();
        emulating_ = false;
        return false;
    }

    if (apdu[0] == 0x29 || apdu[0] == 0x25) {
        emulating_ = false;
        return false;
    }

    if (apdu.size() > 2 && apdu[0] == 0x00 && apdu[1] == 0x00) {
        apdu.erase(apdu.begin());
    }

    emulationExchangeCount_++;
    return respondToApdu(apdu);
}

bool Pn532BleClient::isEmulating() const
{
    return emulating_;
}

int Pn532BleClient::emulationExchangeCount() const
{
    return emulationExchangeCount_;
}

bool Pn532BleClient::extractType2Ndef(const NfcDump& dump, std::vector<uint8_t>& ndef) const
{
    ndef.clear();

    if (!isNtag(dump.card.type) || dump.data.size() <= 16) {
        return false;
    }

    size_t offset = 16;

    while (offset < dump.data.size()) {
        const uint8_t type = dump.data[offset++];

        if (type == 0x00) {
            continue;
        }

        if (type == 0xFE || offset >= dump.data.size()) {
            return false;
        }

        size_t length = dump.data[offset++];

        if (length == 0xFF) {
            if (offset + 1 >= dump.data.size()) {
                return false;
            }

            length = (static_cast<size_t>(dump.data[offset]) << 8) | dump.data[offset + 1];
            offset += 2;
        }

        if (offset + length > dump.data.size()) {
            return false;
        }

        if (type == 0x03) {
            ndef.assign(dump.data.begin() + offset, dump.data.begin() + offset + length);
            return !ndef.empty();
        }

        offset += length;
    }

    return false;
}

std::vector<uint8_t> Pn532BleClient::buildTargetParameters(const NfcDump& dump) const
{
    std::vector<uint8_t> uid;
    hexToBytes(dump.card.uid, uid);

    while (uid.size() < 3) {
        uid.push_back(0x00);
    }

    return {
        0x04,
        0x08, 0x00,
        uid[0], uid[1], uid[2],
        0x60,
        0x01, 0xFE, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xFF, 0xFF,
        0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
        0x00,
        0x00,
    };
}

bool Pn532BleClient::respondToApdu(const std::vector<uint8_t>& apdu)
{
    if (apdu.size() >= 13 && apdu[0] == 0x00 && apdu[1] == 0xA4 && apdu[2] == 0x04) {
        const uint8_t app[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};

        if (memcmp(&apdu[5], app, sizeof(app)) == 0) {
            return sendStatus(0x90, 0x00);
        }
    }

    if (apdu.size() >= 7 && apdu[0] == 0x00 && apdu[1] == 0xA4 && apdu[2] == 0x00) {
        selectedEmuFile_ = (static_cast<uint16_t>(apdu[5]) << 8) | apdu[6];
        return sendStatus(0x90, 0x00);
    }

    if (apdu.size() >= 5 && apdu[0] == 0x00 && apdu[1] == 0xB0) {
        const uint16_t offset = (static_cast<uint16_t>(apdu[2]) << 8) | apdu[3];
        const uint8_t requested = apdu[4];
        const std::vector<uint8_t>* file = nullptr;

        if (selectedEmuFile_ == 0xE103) {
            file = &emulatedCcFile_;
        } else if (selectedEmuFile_ == 0xE104) {
            file = &emulatedNdefFile_;
        }

        if (file == nullptr || offset > file->size()) {
            return sendStatus(0x6A, 0x82);
        }

        const size_t available = file->size() - offset;
        const size_t length = requested < available ? requested : available;
        std::vector<uint8_t> response(file->begin() + offset, file->begin() + offset + length);
        response.push_back(0x90);
        response.push_back(0x00);
        ble_->setData(response);
        return true;
    }

    return sendStatus(0x6D, 0x00);
}

bool Pn532BleClient::sendStatus(uint8_t sw1, uint8_t sw2)
{
    ble_->setData({sw1, sw2});
    return true;
}
