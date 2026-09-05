// Pn532KillerApp - menu models: item counts and labels per view.

#include "Pn532KillerApp.h"

int Pn532KillerApp::mainMenuCount() const
{
    return transportMode_ == TransportMode::Ble ? 5 : 6;
}

String Pn532KillerApp::mainMenuLabel(int index) const
{
    if (index == 0) return "Scan Info";
    if (index == 1) return "Read Auto";
    if (index == 2) return "Saved Dumps";

    if (transportMode_ != TransportMode::Ble) {
        if (index == 3) return "Killer Slots";
        if (index == 4) return "Write Tag";
        return "Lab";
    }

    if (index == 3) return transportMode_ == TransportMode::Ble ? "Emulate NDEF" : "Write Tag";
    return "Lab";
}

int Pn532KillerApp::cardActionCount() const
{
    if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() > 0) {
        return 3;
    }

    if (currentDump_.status == DumpStatus::Empty) {
        return 0;
    }

    if (currentDump_.status == DumpStatus::InfoOnly) {
        return 3;
    }

    if (isMifareClassic(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
        return 5;
    }

    if (isNtag(currentDump_.card.type)) {
        if (transportMode_ == TransportMode::Ble) {
            return 4;
        }

        return currentDump_.status == DumpStatus::Full ? 3 : 4;
    }

    return 3;
}

String Pn532KillerApp::cardActionLabel(int index) const
{
    if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() > 0) {
        const char* items[] = {"Read detected", "Save info", "Back"};
        return items[index];
    }

    if (currentDump_.status == DumpStatus::InfoOnly) {
        const char* items[] = {"Save info", "Raw info", "Back"};
        return items[index];
    }

    if (isMifareClassic(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
        if (transportMode_ == TransportMode::Ble) {
            const char* items[] = {"Save partial", "Retry missing", "Keys", "Write card", "Back"};
            return items[index];
        }

        const char* items[] = {"Save partial", "Retry missing", "Keys", "Write card", "Back"};
        return items[index];
    }

    if (isNtag(currentDump_.card.type)) {
        if (transportMode_ == TransportMode::Ble) {
            if (currentDump_.status == DumpStatus::Full) {
                const char* items[] = {"Save dump", "Emulate full", "Diagnostics", "Back"};
                return items[index];
            }

            const char* items[] = {"Save partial", "Retry read", "Diagnostics", "Back"};
            return items[index];
        }

        if (currentDump_.status == DumpStatus::Partial) {
            const char* items[] = {"Save partial", "Retry read", "Diagnostics", "Back"};
            return items[index];
        }

        const char* items[] = {"Save dump", "Diagnostics", "Back"};
        return items[index];
    }

    if (transportMode_ == TransportMode::Ble) {
        const char* items[] = {"Save dump", "Write card", "Back"};
        return items[index];
    }

    const char* items[] = {"Save dump", "Write card", "Back"};
    return items[index];
}

int Pn532KillerApp::cardInfoLineCount() const
{
    if (currentDump_.status == DumpStatus::Empty) {
        return lastCard_.uid.length() > 0 ? 5 : 1;
    }

    if (currentDump_.card.family == CardFamily::Type2 || isNtag(currentDump_.card.type)) {
        return 11 + currentDump_.reportLines.size();
    }

    int count = 8;

    if (!currentDump_.missingUnits.empty()) {
        count++;
    }

    if (currentDump_.unitsRead == 0 && currentDump_.unitsTotal > 0) {
        count++;
    }

    count += currentDump_.reportLines.size();
    return count;
}

String Pn532KillerApp::cardInfoLine(int index) const
{
    const CardInfo& card = currentDump_.status == DumpStatus::Empty ? lastCard_ : currentDump_.card;

    if (currentDump_.status == DumpStatus::Empty) {
        if (index == 0) return "UID: " + card.uid;
        if (index == 1) return "Type: " + String(cardTypeName(card.type));
        if (index == 2) return "ATQA: " + card.atqa;
        if (index == 3) return "SAK: " + card.sak;
        if (index == 4) return String("Readable: ") + (card.memoryReadable ? "yes" : "info only");
        return "";
    }

    if (currentDump_.card.family == CardFamily::Type2 || isNtag(card.type)) {
        if (index == 0) return "Type: " + (currentDump_.exactType.length() > 0 ? currentDump_.exactType : String(cardTypeName(card.type)));
        if (index == 1) return "UID: " + card.uid;
        if (index == 2) return "Family: " + (currentDump_.familyName.length() > 0 ? currentDump_.familyName : String(cardFamilyName(card.family)));
        if (index == 3) return "Memory: " + String(currentDump_.unitsRead) + "/" + String(currentDump_.unitsTotal) + " pages";
        if (index == 4) return "Status: " + (currentDump_.statusSummary.length() > 0 ? currentDump_.statusSummary : String(dumpStatusName(currentDump_.status)));
        if (index == 5) return "Clone: " + (currentDump_.cloneSummary.length() > 0 ? currentDump_.cloneSummary : String(currentDump_.status == DumpStatus::Full ? "Full dump" : "Partial only"));
        if (index == 6) return "Actions: " + (currentDump_.actionCapabilities.length() > 0 ? currentDump_.actionCapabilities : "Save, Diagnostics");
        if (index == 7) return "Protected: " + String(currentDump_.protectedUnits);
        if (index == 8) return "Unknown: " + String(currentDump_.unknownUnits);
        if (index == 9) return "ATQA/SAK: " + card.atqa + "/" + card.sak;
        if (index == 10) return "Method: " + (currentDump_.readMethod.length() > 0 ? currentDump_.readMethod : "unknown");

        const int reportIndex = index - 11;

        if (reportIndex >= 0 && reportIndex < static_cast<int>(currentDump_.reportLines.size())) {
            return currentDump_.reportLines[reportIndex];
        }

        return "";
    }

    if (index == 0) return "UID: " + card.uid;
    if (index == 1) return "Type: " + String(cardTypeName(card.type));
    if (index == 2) return "Status: " + String(dumpStatusName(currentDump_.status));
    if (index == 3) return "Read: " + String(currentDump_.unitsRead) + "/" + String(currentDump_.unitsTotal);
    if (index == 4) return "Bytes: " + String(currentDump_.data.size());
    if (index == 5) return "ATQA: " + card.atqa;
    if (index == 6) return "SAK: " + card.sak;
    if (index == 7) return String("Emulate: ") + (card.canEmulate ? "yes" : "no");

    int extraIndex = 8;

    if (currentDump_.unitsRead == 0 && currentDump_.unitsTotal > 0) {
        if (index == extraIndex) return "Memory locked/protected";
        extraIndex++;
    }

    if (!currentDump_.missingUnits.empty() && index == extraIndex) {
        String line = isNtag(card.type) ? "Failed page: " : "Missing: ";
        const int shown = min(5, static_cast<int>(currentDump_.missingUnits.size()));

        for (int i = 0; i < shown; i++) {
            if (i > 0) {
                line += ",";
            }

            line += String(currentDump_.missingUnits[i]);
        }

        if (static_cast<int>(currentDump_.missingUnits.size()) > shown) {
            line += "...";
        }

        return line;
    }

    if (!currentDump_.missingUnits.empty()) {
        extraIndex++;
    }

    const int reportIndex = index - extraIndex;

    if (reportIndex >= 0 && reportIndex < static_cast<int>(currentDump_.reportLines.size())) {
        return currentDump_.reportLines[reportIndex];
    }

    return "";
}

int Pn532KillerApp::dumpActionCount() const
{
    if (currentDump_.status == DumpStatus::InfoOnly) {
        return 3;
    }

    if (transportMode_ == TransportMode::Ble) {
        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Full) {
            return 4;
        }

        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
            return 4;
        }

        if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
            return 3;
        }

        return 5;
    }

    if (isNtag(currentDump_.card.type)) {
        return currentDump_.status == DumpStatus::Partial ? 3 : 2;
    }

    if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
        return 3;
    }

    return 5;
}

String Pn532KillerApp::dumpActionLabel(int index) const
{
    if (currentDump_.status == DumpStatus::InfoOnly) {
        const char* items[] = {"Raw info", "Delete", "Back"};
        return items[index];
    }

    if (transportMode_ == TransportMode::Ble) {
        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Full) {
            const char* items[] = {"Emulate dump", "Custom NDEF", "Delete", "Back"};
            return items[index];
        }

        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
            const char* items[] = {"Retry read", "Diag read", "Delete", "Back"};
            return items[index];
        }

        if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
            const char* items[] = {"Custom NDEF", "Delete", "Back"};
            return items[index];
        }

        const char* items[] = {"View missing", "Retry read", "Custom NDEF", "Delete", "Back"};
        return items[index];
    }

    if (isNtag(currentDump_.card.type)) {
        if (currentDump_.status == DumpStatus::Partial) {
            const char* items[] = {"Retry read", "Diag read", "Delete", "Back"};
            return items[index];
        }

        const char* items[] = {"Delete", "Back"};
        return items[index];
    }

    if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
        const char* items[] = {"Write card", "Delete", "Back"};
        return items[index];
    }

    const char* items[] = {"View missing", "Retry read", "Write card", "Delete", "Back"};
    return items[index];
}

int Pn532KillerApp::missingUnitLineCount() const
{
    if (currentDump_.missingUnits.empty()) {
        return 2;
    }

    return 1 + ((static_cast<int>(currentDump_.missingUnits.size()) + 7) / 8);
}

String Pn532KillerApp::missingUnitLine(int index) const
{
    if (index == 0) {
        if (currentDump_.missingUnits.empty()) {
            return isNtag(currentDump_.card.type) ? "No failed pages" : "No missing sectors";
        }

        return String(isNtag(currentDump_.card.type) ? "Failed pages: " : "Missing: ") + String(currentDump_.missingUnits.size());
    }

    if (currentDump_.missingUnits.empty()) {
        return index == 1 ? "Retry not needed" : "";
    }

    const int start = (index - 1) * 8;
    String line = "";

    for (int i = start; i < start + 8 && i < static_cast<int>(currentDump_.missingUnits.size()); i++) {
        if (line.length() > 0) {
            line += ",";
        }

        line += String(currentDump_.missingUnits[i]);
    }

    return line;
}
