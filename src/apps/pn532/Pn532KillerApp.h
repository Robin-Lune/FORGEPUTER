#pragma once

#include "../../core/App.h"
#include "../../core/PowerManager.h"
#include "../../nfc/NdefMessageBuilder.h"
#include "../../nfc/NdefWriter.h"
#include "../../nfc/ReadAutoController.h"
#include "../../pn532/Pn532BleClient.h"
#include "../../pn532/Pn532KillerClient.h"
#include "../../pn532/Pn532UartTransport.h"
#include "../../pn532/Pn532UsbCdcTransport.h"
#include "../../storage/DumpStore.h"
#include "../../storage/KeyStore.h"

class Pn532KillerApp : public App {
public:
    explicit Pn532KillerApp(PowerManager& powerManager);

    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    enum class View {
        Transport,
        Menu,
        ScanInfo,
        ReadProgress,
        CardInfo,
        CardActions,
        RetryResult,
        SaveName,
        DumpList,
        DumpDetail,
        MissingUnits,
        BleEmulate,
        EmulateNdefEdit,
        Slots,
        WriteTag,
        Lab,
    };

    enum class TransportMode {
        None,
        Uart,
        UsbCdc,
        Ble,
    };

    static constexpr int transportCount_ = 3;
    static constexpr int visibleItemCount_ = 3;
    static constexpr int slotTypeCount_ = 4;
    static constexpr uint32_t bleEmulateDurationMs_ = 120000;
    const char* transportItems_[transportCount_] = {
        "UART direct",
        "USB-C serial",
        "BLE bridge",
    };
    PowerManager& powerManager_;
    Pn532UartTransport transport_;
    Pn532UsbCdcTransport usbTransport_;
    Pn532Client pn532_;
    Pn532KillerClient killer_;
    KeyStore keyStore_;
    DumpStore dumpStore_;
    ReadAutoController reader_;
    NdefWriter ndefWriter_;
    Pn532BleClient bleClient_;
    View view_ = View::Transport;
    TransportMode transportMode_ = TransportMode::None;
    int selectedIndex_ = 0;
    int listOffset_ = 0;
    bool pn532Ready_ = false;
    String status_;
    CardInfo lastCard_;
    NfcDump currentDump_;
    NfcReadProgress readProgress_;
    std::vector<String> readLog_;
    int resultOffset_ = 0;
    String saveName_;
    std::vector<DumpEntry> dumpEntries_;
    int selectedDump_ = 0;
    uint8_t selectedSlot_ = 0;
    KillerSlotType selectedSlotType_ = KillerSlotType::Mfc1K;
    bool slotLoaded_[slotTypeCount_][8] = {};
    String slotNames_[slotTypeCount_][8];
    String slotUids_[slotTypeCount_][8];
    String writeBuffer_ = "https://forgeputer.local";
    bool writeUrlMode_ = true;
    bool emulateUrlMode_ = true;
    String emulateBuffer_ = "https://forgeputer.local";
    bool emulateFromDump_ = false;
    bool selectingDumpForSlot_ = false;
    bool pendingSlotUpload_ = false;
    bool slotUploadResultVisible_ = false;
    bool ntagDiagnosticVisible_ = false;
    View diagnosticReturnView_ = View::CardActions;
    int readLogOffset_ = 0;
    uint32_t bleEmulateStartedAt_ = 0;
    uint32_t bleEmulateLastDrawAt_ = 0;
    uint32_t bleEmulateNextAttemptAt_ = 0;
    bool bleEmulatePending_ = false;
    bool bleEmulatePendingUseDump_ = false;
    std::vector<uint8_t> bleEmulatePendingRecord_;
    CardInfo bleEmulatePendingCard_;
    int retryBeforeRead_ = 0;
    int retryAfterRead_ = 0;
    int retryBeforeMissing_ = 0;
    int retryAfterMissing_ = 0;

    void initStorage();
    void initPn532();
    void releaseBleConnection();
    void moveSelection(int delta);
    void openSelectedItem();
    void goBack();
    void drawTransport();
    void drawMenu();
    void drawScanInfo();
    void drawReadProgress();
    void drawCardInfo();
    void drawCardActions();
    void drawRetryResult();
    void drawSaveName();
    void drawDumpList();
    void drawDumpDetail();
    void drawMissingUnits();
    void drawBleEmulate();
    void drawEmulateNdefEdit();
    void drawSlots();
    void drawWriteTag();
    void drawLab();
    void drawBattery();
    void drawList(const char* const* items, int count, int selected, int offset, int startY);
    void drawCardActionList(int startY);
    void drawDumpActionList(int startY);
    void drawScrollHints(int count, int offset, int startY);
    void drawScrollHints(int count, int offset, int startY, int visibleCount);
    void updateListOffset(int count);
    int visibleRowsForCurrentView() const;
    int mainMenuCount() const;
    String mainMenuLabel(int index) const;
    void openMainMenuItem();
    int cardActionCount() const;
    String cardActionLabel(int index) const;
    int cardInfoLineCount() const;
    String cardInfoLine(int index) const;
    int dumpActionCount() const;
    String dumpActionLabel(int index) const;
    int missingUnitLineCount() const;
    String missingUnitLine(int index) const;
    bool isMifareClassic(CardType type) const;
    bool isNtag(CardType type) const;
    void selectTransport();
    bool ensureDirectReady();
    bool ensureBleReady();
    void scanInfo();
    void readAuto();
    void beginReadProgress(const String& title);
    void handleReadProgress(const NfcReadProgress& progress);
    static void handleReadProgressThunk(void* context, const NfcReadProgress& progress);
    void saveCurrentDump();
    void openSaveName();
    String suggestedDumpName() const;
    void prepareInfoDumpFromScan();
    void performCardAction();
    void performDumpAction();
    void retryMissing();
    void runNtagDiagnostic(View returnView);
    void ntagDiagnosticCommand(const String& label, const std::vector<uint8_t>& tagCommand);
    void refreshDumpList();
    void loadSelectedDump();
    void openSlotDumpPicker();
    void loadSelectedDumpForSlot();
    void deleteSelectedDump();
    void startBleNdefEmulation();
    void openBleNdefEditor(bool fromDump);
    void startBleEditedNdefEmulation();
    void stopBleNdefEmulation();
    int bleEmulateSecondsLeft() const;
    void prepareBleEmulationWindow(bool useDump, const std::vector<uint8_t>& record, const CardInfo& card);
    void writeCurrentNdef();
    void switchSelectedSlot();
    void startSlot();
    void stopSlot();
    bool uploadCurrentDumpToSlot();
    void runRawPn532Diagnostic();
    bool runRawScenario(const String& label, const std::vector<uint8_t>& samCommand, bool rfReset, bool maxRetries, bool inSelect, bool communicateThru, CardInfo& info);
    Pn532RawDiagnostic diagnoseTransportRaw(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs);
    uint8_t targetFromInList(const Pn532RawDiagnostic& diag, CardInfo& info);
    bool runPolledRawDiagnostic(const String& label, const std::vector<uint8_t>& tagCommand, bool communicateThru, uint16_t timeoutMs, CardInfo& info);
    void appendRawDiagnostic(const String& label, const Pn532RawDiagnostic& diag);
    bool saveRawDiagnosticLog(const CardInfo& info, String& savedPath);
    int slotTypeIndex(KillerSlotType type) const;
    KillerSlotType slotTypeForIndex(int index) const;
    void cycleSlotType();
    KillerSlotType slotTypeForDump(const NfcDump& dump) const;
    const char* killerSlotTypeName(KillerSlotType type) const;
    void showStatus(const String& status);
    bool hasCurrentDump() const;
};
