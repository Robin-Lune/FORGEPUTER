// Pn532KillerApp - SD log files for raw diagnostics and slot uploads.

#include "Pn532KillerApp.h"

#include <SD.h>

bool Pn532KillerApp::saveRawDiagnosticLog(const CardInfo& info, String& savedPath)
{
    savedPath = "";

    if (SD.cardType() == CARD_NONE) {
        return false;
    }

    SD.mkdir("/forgeputer");
    SD.mkdir("/forgeputer/nfc");
    SD.mkdir("/forgeputer/nfc/logs");

    if (!SD.exists("/forgeputer/nfc/logs")) {
        return false;
    }

    String uid = info.uid;
    uid.replace(":", "");
    uid.toLowerCase();

    if (uid.length() == 0) {
        uid = "unknown";
    }

    savedPath = "/forgeputer/nfc/logs/raw_" + uid + "_" + String(millis()) + ".txt";
    File file = SD.open(savedPath, FILE_WRITE);

    if (!file) {
        savedPath = "";
        return false;
    }

    file.println("Forgeputer PN532 raw diagnostic");
    file.print("transport: ");
    file.println(transportMode_ == TransportMode::UsbCdc ? "usb-cdc" : "uart");
    file.print("uid: ");
    file.println(info.uid);
    file.print("atqa: ");
    file.println(info.atqa);
    file.print("sak: ");
    file.println(info.sak);
    file.print("target: ");
    file.println(pn532_.selectedTargetNumber());
    file.println();

    for (const String& line : readLog_) {
        file.println(line);
    }

    file.close();
    return true;
}

bool Pn532KillerApp::saveSlotUploadLog(KillerSlotType type, uint8_t slot, bool uploaded, bool emulationStarted, String& savedPath)
{
    savedPath = "";

    if (SD.cardType() == CARD_NONE) {
        return false;
    }

    SD.mkdir("/forgeputer");
    SD.mkdir("/forgeputer/nfc");
    SD.mkdir("/forgeputer/nfc/logs");

    if (!SD.exists("/forgeputer/nfc/logs")) {
        return false;
    }

    String dumpName = currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName();
    dumpName.toLowerCase();
    String safeName;

    for (size_t i = 0; i < dumpName.length(); i++) {
        const char c = dumpName[i];

        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            safeName += c;
        } else if (c == ' ' || c == '.' || c == ':') {
            safeName += "_";
        }
    }

    if (safeName.length() == 0) {
        safeName = "unknown";
    }

    String typeName = killerSlotTypeName(type);
    typeName.toLowerCase();
    typeName.replace(" ", "_");
    savedPath = "/forgeputer/nfc/logs/slot_" + safeName + "_" + typeName + "_s" + String(slot + 1) + "_" + String(millis()) + ".txt";
    File file = SD.open(savedPath, FILE_WRITE);

    if (!file) {
        savedPath = "";
        return false;
    }

    file.println("Forgeputer PN532Killer slot upload");
    file.print("transport: ");
    file.println(transportMode_ == TransportMode::UsbCdc ? "usb-cdc" : "uart");
    file.print("dump: ");
    file.println(currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName());
    file.print("card_type: ");
    file.println(cardTypeSlug(currentDump_.card.type));
    file.print("slot_type: ");
    file.println(killerSlotTypeName(type));
    file.print("slot: ");
    file.println(slot + 1);
    file.print("bytes: ");
    file.println(currentDump_.data.size());
    file.print("uploaded: ");
    file.println(uploaded ? "yes" : "no");
    file.print("emulation_started: ");
    file.println(emulationStarted ? "yes" : "no");
    file.println();

    for (const String& line : readLog_) {
        file.println(line);
    }

    file.close();
    return true;
}
