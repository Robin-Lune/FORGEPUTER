// Pn532Client - low level frame IO on the active transport.

#include "Pn532Client.h"

#include "Pn532Protocol.h"

using Pn532Protocol::ackFrame;
using Pn532Protocol::checksum;
using Pn532Protocol::hostToPn532;

bool Pn532Client::sendFrame(const std::vector<uint8_t>& command)
{
    if (command.size() > 253) {
        setError("Frame too long");
        return false;
    }

    std::vector<uint8_t> frame;
    const uint8_t length = command.size() + 1;
    uint8_t dataSum = hostToPn532;

    frame.reserve(command.size() + 8);
    frame.push_back(0x00);
    frame.push_back(0x00);
    frame.push_back(0xFF);
    frame.push_back(length);
    frame.push_back(checksum(length));
    frame.push_back(hostToPn532);

    for (uint8_t byte : command) {
        frame.push_back(byte);
        dataSum += byte;
    }

    frame.push_back(checksum(dataSum));
    frame.push_back(0x00);
    transport_->write(frame.data(), frame.size());
    transport_->flush();
    return true;
}

bool Pn532Client::readAck(uint16_t timeoutMs)
{
    for (uint8_t expected : ackFrame) {
        const int value = readByte(timeoutMs);

        if (value < 0 || static_cast<uint8_t>(value) != expected) {
            if (value < 0) {
                setError("PN532 no ACK timeout");
            } else {
                setError("PN532 bad ACK " + String(value, HEX));
            }

            return false;
        }
    }

    return true;
}

bool Pn532Client::readFrame(std::vector<uint8_t>& frame, uint16_t timeoutMs)
{
    frame.clear();

    int preamble = readByte(timeoutMs);

    while (preamble >= 0 && preamble != 0x00) {
        preamble = readByte(timeoutMs);
    }

    if (preamble < 0) {
        setError("Response timeout");
        return false;
    }

    if (readByte(timeoutMs) != 0x00 || readByte(timeoutMs) != 0xFF) {
        setError("Bad preamble");
        return false;
    }

    const int length = readByte(timeoutMs);
    const int lcs = readByte(timeoutMs);

    if (length < 0 || lcs < 0 || static_cast<uint8_t>(length + lcs) != 0) {
        setError("Bad length");
        return false;
    }

    uint8_t sum = 0;

    for (int i = 0; i < length; i++) {
        const int value = readByte(timeoutMs);

        if (value < 0) {
            setError("Short frame");
            return false;
        }

        frame.push_back(value);
        sum += value;
    }

    const int dcs = readByte(timeoutMs);
    const int postamble = readByte(timeoutMs);

    if (dcs < 0 || postamble < 0 || static_cast<uint8_t>(sum + dcs) != 0 || postamble != 0x00) {
        setError("Bad checksum");
        return false;
    }

    return true;
}

int Pn532Client::readByte(uint16_t timeoutMs)
{
    const unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        if (transport_->available() > 0) {
            return transport_->read();
        }

        delay(1);
    }

    return -1;
}

void Pn532Client::drainInput()
{
    const uint32_t start = millis();

    while (millis() - start < 20) {
        while (transport_->available() > 0) {
            transport_->read();
        }

        delay(1);
    }
}

void Pn532Client::setError(const String& message)
{
    lastError_ = message;
}
