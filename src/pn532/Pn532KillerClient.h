#pragma once

#include "../nfc/CardInfo.h"
#include "Pn532Client.h"

#include <vector>

enum class KillerSlotType {
    Unknown,
    Mfc1K,
    Ntag,
    Iso15693,
    Em4100,
};

struct KillerSlotInfo {
    uint8_t index = 0;
    KillerSlotType type = KillerSlotType::Unknown;
    String uid;
    bool loaded = false;
};

// Implementation split across Pn532KillerClient*.cpp:
//   Pn532KillerClient.cpp        slots, emulation control, upload dispatch
//   Pn532KillerClientUpload.cpp  upload strategies and WR/RD traces
// Shared vendor constants live in Pn532KillerProtocol.h.
class Pn532KillerClient {
public:
    explicit Pn532KillerClient(Pn532Client& pn532);

    void setProgressCallback(void* context, NfcReadProgressCallback callback);
    bool switchMifareSlot(uint8_t slot);
    bool switchNtagSlot(uint8_t slot);
    bool switchIso15693Slot(uint8_t slot);
    bool switchEm4100Slot(uint8_t slot);
    bool startEmulation();
    bool startEmulation(uint8_t slot, KillerSlotType type);
    bool stopEmulation();
    bool readSlotInfo(uint8_t slot, KillerSlotInfo& info);
    bool uploadDump(uint8_t slot, const NfcDump& dump);
    String lastError() const;
    const std::vector<String>& traceLines() const;

private:
    Pn532Client& pn532_;
    String lastError_;
    std::vector<String> traceLines_;
    void* progressContext_ = nullptr;
    NfcReadProgressCallback progressCallback_ = nullptr;
    uint8_t activeSlot_ = 0;
    KillerSlotType activeType_ = KillerSlotType::Mfc1K;

    bool setWorkMode(uint8_t mode, uint8_t type, uint8_t slot);
    bool uploadMifareClassic1K(uint8_t slot, const NfcDump& dump);
    bool uploadNtagPages(uint8_t slot, const NfcDump& dump, int pageCount);
    bool uploadNtagWindows(uint8_t slot, const NfcDump& dump, int pageCount);
    bool uploadLinearUnits(uint8_t slot, KillerSlotType type, const NfcDump& dump, size_t unitSize, int unitCount, const String& label);
    bool uploadAddressedUnits(uint8_t slot, KillerSlotType type, const NfcDump& dump, size_t unitSize, int unitCount, const String& label);
    bool writeEmulatorData(uint8_t type, uint8_t slot, uint16_t index, const uint8_t* data, size_t length);
    bool readEmulatorData(uint8_t type, uint8_t slot, uint16_t index, std::vector<uint8_t>& response);
    bool completeEmulatorWrite(uint8_t type, uint8_t slot);
    bool emulatorResponseOk(const std::vector<uint8_t>& response, const String& context);
    bool shouldTraceIndex(uint16_t index) const;
    void traceCommand(const String& prefix, uint8_t type, uint8_t slot, uint16_t index, const uint8_t* data, size_t length, const std::vector<uint8_t>& response);
    void traceReadback(uint8_t type, uint8_t slot, uint16_t index);
    uint8_t killerType(KillerSlotType type) const;
    KillerSlotType slotTypeForDump(const NfcDump& dump) const;
    void emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok);
    void setError(const String& error);
};
