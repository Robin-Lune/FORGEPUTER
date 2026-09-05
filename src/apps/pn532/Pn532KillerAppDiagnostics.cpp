// Pn532KillerApp - MFU/NTAG and raw PN532 diagnostics.

#include "Pn532KillerApp.h"

void Pn532KillerApp::runNtagDiagnostic(View returnView)
{
    diagnosticReturnView_ = returnView;
    ntagDiagnosticVisible_ = true;
    readLogOffset_ = 0;
    beginReadProgress("MFU Diagnostic");
    readProgress_.title = "MFU Diagnostic";
    readProgress_.detail = currentDump_.card.uid.length() > 0 ? currentDump_.card.uid : "raw tests";
    readProgress_.total = 5;
    readProgress_.current = 0;
    readLog_.clear();
    drawReadProgress();

    if (!isNtag(currentDump_.card.type)) {
        readLog_.push_back("!! Not MFU/NTAG");
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    if (transportMode_ == TransportMode::Ble) {
        readLog_.push_back("!! Diag wired only");
        readLog_.push_back("Use UART/USB-C");
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    if (!ensureDirectReady()) {
        readLog_.push_back("!! PN532 not ready");
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    CardInfo info;
    if (!reader_.scan(info)) {
        readLog_.push_back("!! Scan " + reader_.lastError());
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    readLog_.push_back("Target " + String(pn532_.selectedTargetNumber()) + " " + info.uid);

    ntagDiagnosticCommand("GET_VERSION", {0x60});
    ntagDiagnosticCommand("READ 0", {0x30, 0x00});
    ntagDiagnosticCommand("READ 4", {0x30, 0x04});
    ntagDiagnosticCommand("FAST 0-3", {0x3A, 0x00, 0x03});
    ntagDiagnosticCommand("READ_SIG", {0x3C, 0x00});
    readProgress_.detail = "Done";
    readProgress_.done = true;
    drawReadProgress();
}

void Pn532KillerApp::ntagDiagnosticCommand(const String& label, const std::vector<uint8_t>& tagCommand)
{
    std::vector<uint8_t> command = {0x40, pn532_.selectedTargetNumber()};
    command.insert(command.end(), tagCommand.begin(), tagCommand.end());

    std::vector<uint8_t> response;
    const bool ok = pn532_.rawCommand(command, response, 1500);
    readProgress_.current++;

    if (!ok) {
        readLog_.push_back("!! " + label + " " + pn532_.lastError());
        drawReadProgress();
        return;
    }

    if (response.empty()) {
        readLog_.push_back("!! " + label + " empty");
        drawReadProgress();
        return;
    }

    String line = "OK " + label;

    if (response[0] == 0x00 && response.size() > 1) {
        line += " ";
        line += bytesToHex(&response[1], min(static_cast<size_t>(8), response.size() - 1), false);
    } else if (response[0] == 0x00) {
        line += " status0";
    } else {
        line += " raw ";
        line += bytesToHex(response.data(), min(static_cast<size_t>(8), response.size()), false);
    }

    readLog_.push_back(line);
    drawReadProgress();
}

void Pn532KillerApp::runRawPn532Diagnostic()
{
    if (transportMode_ == TransportMode::Ble) {
        if (!ensureBleReady()) {
            return;
        }
    } else if (!ensureDirectReady()) {
        return;
    }

    ntagDiagnosticVisible_ = true;
    diagnosticReturnView_ = View::Lab;
    readLogOffset_ = 0;
    beginReadProgress("Raw PN532");
    readProgress_.title = "Raw PN532";
    readProgress_.detail = "Compare transports";
    readProgress_.total = 27;
    readProgress_.current = 0;
    readLog_.clear();
    rawDiagLegacyIdxFailed_ = false;
    rawDiagType2CrcOk_ = false;
    readLog_.push_back(String("Transport ") + (transportMode_ == TransportMode::Ble ? "BLE" : (transportMode_ == TransportMode::UsbCdc ? "USB-CDC" : "UART")));
    drawReadProgress();

    CardInfo info;

    runRawScenario("A SAM min IDX", {0x14, 0x01}, false, false, false, false, info);
    runRawScenario("B SAM Forge IDX", {0x14, 0x01, 0x14, 0x01}, false, false, false, false, info);
    runRawScenario("C RF SAM min IDX", {0x14, 0x01}, true, false, false, false, info);
    runRawScenario("D RF SAM min ICT", {0x14, 0x01}, true, false, false, true, info);
    runRawScenario("E SAM min Select IDX", {0x14, 0x01}, false, false, true, false, info);
    runRawScenario("F MaxRetries IDX", {0x14, 0x01}, false, true, false, false, info);
    runType2RawCrcDiagnostic(info);

    readProgress_.detail = "Done";

    String savedPath;
    if (saveRawDiagnosticLog(info, savedPath)) {
        readLog_.push_back("Log saved");
        readLog_.push_back(savedPath);
    } else {
        readLog_.push_back("Log not saved");
    }

    readProgress_.done = true;
    drawReadProgress();
}

bool Pn532KillerApp::runRawScenario(const String& label, const std::vector<uint8_t>& samCommand, bool rfReset, bool maxRetries, bool inSelect, bool communicateThru, CardInfo& info)
{
    readLog_.push_back("-- " + label + " --");

    if (rfReset) {
        readProgress_.detail = label + " RF off";
        appendRawDiagnostic(label + " RF off", diagnoseTransportRaw({0x32, 0x01, 0x00}, 0x33, "RFConfiguration", false, 500));
        readProgress_.current++;
        delay(50);
        readProgress_.detail = label + " RF on";
        appendRawDiagnostic(label + " RF on", diagnoseTransportRaw({0x32, 0x01, 0x01}, 0x33, "RFConfiguration", false, 500));
        readProgress_.current++;
    }

    readProgress_.detail = label + " SAM";
    appendRawDiagnostic(label + " SAM", diagnoseTransportRaw(samCommand, 0x15, "SAMConfiguration", false, 500));
    readProgress_.current++;

    if (maxRetries) {
        readProgress_.detail = label + " MaxRetries";
        appendRawDiagnostic(label + " MaxRetries", diagnoseTransportRaw({0x32, 0x05, 0xFF, 0xFF, 0xFF}, 0x33, "RFConfiguration", false, 500));
        readProgress_.current++;
    }

    readProgress_.detail = label + " InList";
    const Pn532RawDiagnostic poll = diagnoseTransportRaw({0x4A, 0x01, 0x00}, 0x4B, "InListPassiveTarget", false, 500);
    appendRawDiagnostic(label + " InList", poll);
    readProgress_.current++;

    const uint8_t target = targetFromInList(poll, info);
    readLog_.push_back("Target " + String(target));

    if (inSelect) {
        readProgress_.detail = label + " InSelect";
        appendRawDiagnostic(label + " InSelect", diagnoseTransportRaw({0x54, target}, 0x55, "InSelect", true, 500));
        readProgress_.current++;
    }

    readProgress_.detail = label + " READ 04";
    const std::vector<uint8_t> readCommand = communicateThru
        ? std::vector<uint8_t>{0x42, 0x30, 0x04, 0x26, 0xEE}
        : std::vector<uint8_t>{0x40, target, 0x30, 0x04};
    const Pn532RawDiagnostic readDiag = diagnoseTransportRaw(readCommand, communicateThru ? 0x43 : 0x41, communicateThru ? "InCommunicateThru" : "InDataExchange", true, 500);
    appendRawDiagnostic(label + " READ 04", readDiag);

    if (!communicateThru && (!readDiag.responseCodeOk || !readDiag.hasStatus || readDiag.inDataExchangeStatus != 0x00)) {
        rawDiagLegacyIdxFailed_ = true;
    }

    readProgress_.current++;
    drawReadProgress();
    return true;
}

bool Pn532KillerApp::runType2RawCrcDiagnostic(CardInfo& info)
{
    const String label = "G Type2 RAW CRC";
    readLog_.push_back("-- " + label + " --");
    readProgress_.detail = label + " SAM";
    appendRawDiagnostic(label + " SAM", diagnoseTransportRaw({0x14, 0x01}, 0x15, "SAMConfiguration", false, 500));
    readProgress_.current++;

    readProgress_.detail = label + " InList";
    const Pn532RawDiagnostic poll = diagnoseTransportRaw({0x4A, 0x01, 0x00}, 0x4B, "InListPassiveTarget", false, 500);
    appendRawDiagnostic(label + " InList", poll);
    readProgress_.current++;

    const uint8_t target = targetFromInList(poll, info);
    readLog_.push_back("Target " + String(target));
    readProgress_.detail = label + " READ 04";
    const Pn532RawDiagnostic rawDiag = diagnoseTransportRaw({0x42, 0x30, 0x04, 0x26, 0xEE}, 0x43, "InCommunicateThru", true, 500);
    appendType2RawCrcDiagnostic("Type2 RAW CRC READ 04", rawDiag);
    rawDiagType2CrcOk_ = rawDiag.responseCodeOk && rawDiag.hasStatus && rawDiag.inDataExchangeStatus == 0x00 && rawDiag.tagPayload.size() >= 2;

    if (rawDiagLegacyIdxFailed_ && rawDiagType2CrcOk_) {
        readLog_.push_back("Conclusion legacy IDX failed / raw CRC OK");
    } else if (rawDiagType2CrcOk_) {
        readLog_.push_back("Conclusion raw CRC OK");
    }

    readProgress_.current++;
    drawReadProgress();
    return true;
}

Pn532RawDiagnostic Pn532KillerApp::diagnoseTransportRaw(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs)
{
    if (transportMode_ == TransportMode::Ble) {
        return bleClient_.diagnoseRawCommand(command, expectedResponseCode, label, hasStatusByte, timeoutMs);
    }

    return pn532_.diagnoseRawCommand(command, expectedResponseCode, label, hasStatusByte, timeoutMs);
}

uint8_t Pn532KillerApp::targetFromInList(const Pn532RawDiagnostic& diag, CardInfo& info)
{
    if (diag.pn532Payload.size() < 6 || diag.pn532Payload[0] == 0) {
        return 0x01;
    }

    const uint8_t nbTg = diag.pn532Payload[0];
    const uint8_t target = diag.pn532Payload[1];
    const uint16_t atqa = (static_cast<uint16_t>(diag.pn532Payload[2]) << 8) | diag.pn532Payload[3];
    const uint8_t sak = diag.pn532Payload[4];
    const uint8_t uidLength = diag.pn532Payload[5];
    readLog_.push_back("NbTg " + String(nbTg));
    readLog_.push_back("Tg " + String(target));
    readLog_.push_back("SENS_RES " + bytesToHex(&diag.pn532Payload[2], 2, true));
    readLog_.push_back("SEL_RES " + bytesToHex(&diag.pn532Payload[4], 1, true));
    readLog_.push_back("NFCIDLen " + String(uidLength));

    if (diag.pn532Payload.size() >= static_cast<size_t>(6 + uidLength)) {
        info.type = detectIso14443AType(atqa, sak);
        info.uid = bytesToHex(&diag.pn532Payload[6], uidLength, true);
        info.atqa = bytesToHex(&diag.pn532Payload[2], 2);
        info.sak = bytesToHex(&diag.pn532Payload[4], 1);
        readLog_.push_back("UID " + info.uid);
        readLog_.push_back("ATQA " + info.atqa + " SAK " + info.sak);
    }

    return target == 0 ? 0x01 : target;
}

bool Pn532KillerApp::runPolledRawDiagnostic(const String& label, const std::vector<uint8_t>& tagCommand, bool communicateThru, uint16_t timeoutMs, CardInfo& info)
{
    readProgress_.detail = label;
    drawReadProgress();

    CardInfo scanned;

    if (!reader_.scan(scanned)) {
        readLog_.push_back("== " + label + " ==");
        readLog_.push_back("POLL ERR " + reader_.lastError());
        readProgress_.current++;
        drawReadProgress();
        return false;
    }

    info = scanned;
    readLog_.push_back("Poll UID " + scanned.uid);
    readLog_.push_back("ATQA " + scanned.atqa + " SAK " + scanned.sak);
    readLog_.push_back("Target " + String(pn532_.selectedTargetNumber()));

    const Pn532RawDiagnostic diag = communicateThru
        ? pn532_.diagnoseInCommunicateThru(tagCommand, timeoutMs)
        : pn532_.diagnoseInDataExchange(tagCommand, timeoutMs);

    appendRawDiagnostic(label, diag);
    readProgress_.current++;
    drawReadProgress();
    return diag.ackOk && diag.frameOk && diag.responseCodeOk && diag.inDataExchangeStatus == 0x00;
}

void Pn532KillerApp::appendRawDiagnostic(const String& label, const Pn532RawDiagnostic& diag)
{
    readLog_.push_back("== " + label + " ==");
    readLog_.push_back("TX " + (diag.txFrame.empty() ? String("-") : bytesToHex(diag.txFrame.data(), diag.txFrame.size(), true)));
    readLog_.push_back(String("ACK ") + (diag.ackOk ? "yes " : "no ") + (diag.ackBytes.empty() ? String("-") : bytesToHex(diag.ackBytes.data(), diag.ackBytes.size(), true)));
    readLog_.push_back("RX " + (diag.rxFrame.empty() ? String("-") : bytesToHex(diag.rxFrame.data(), diag.rxFrame.size(), true)));
    readLog_.push_back("Frame " + (diag.framePayload.empty() ? String("-") : bytesToHex(diag.framePayload.data(), diag.framePayload.size(), true)));
    readLog_.push_back("PN532 " + (diag.pn532Payload.empty() ? String("-") : bytesToHex(diag.pn532Payload.data(), diag.pn532Payload.size(), true)));
    readLog_.push_back("Status " + (!diag.hasStatus ? String("-") : String(diag.inDataExchangeStatus, HEX)));
    readLog_.push_back("Tag " + (diag.tagPayload.empty() ? String("-") : bytesToHex(diag.tagPayload.data(), diag.tagPayload.size(), true)));
    readLog_.push_back("Parsed " + (diag.parsed.length() > 0 ? diag.parsed : diag.error));
    readLog_.push_back("Err " + (diag.error.length() > 0 ? diag.error : String("-")));
    readLog_.push_back("Time " + String(diag.elapsedMs) + "ms");
    drawReadProgress();
}

void Pn532KillerApp::appendType2RawCrcDiagnostic(const String& label, const Pn532RawDiagnostic& diag)
{
    appendRawDiagnostic(label, diag);

    if (!diag.responseCodeOk || !diag.hasStatus) {
        readLog_.push_back("Type2 parsed unavailable");
        drawReadProgress();
        return;
    }

    readLog_.push_back("D5 43 status " + String(diag.inDataExchangeStatus, HEX));

    if (diag.inDataExchangeStatus != 0x00) {
        readLog_.push_back("Type2 status not OK");
        drawReadProgress();
        return;
    }

    if (diag.tagPayload.size() < 2) {
        readLog_.push_back("Type2 payload missing CRC");
        drawReadProgress();
        return;
    }

    const size_t usefulSize = diag.tagPayload.size() - 2;
    readLog_.push_back("Useful " + (usefulSize == 0 ? String("-") : bytesToHex(diag.tagPayload.data(), usefulSize, true)));
    readLog_.push_back("CRC removed " + bytesToHex(&diag.tagPayload[usefulSize], 2, true));
    drawReadProgress();
}
