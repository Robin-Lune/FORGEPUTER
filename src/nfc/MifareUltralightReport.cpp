// MifareUltralightReader - version classification and Type 2 capability report.

#include "MifareUltralightReader.h"

#include <cstdlib>

namespace {
String pageRanges(const std::vector<uint8_t>& pages)
{
    if (pages.empty()) {
        return "-";
    }

    String out;
    int start = pages[0];
    int previous = pages[0];

    for (size_t i = 1; i <= pages.size(); i++) {
        if (i < pages.size() && pages[i] == previous + 1) {
            previous = pages[i];
            continue;
        }

        if (out.length() > 0) {
            out += ",";
        }

        out += String(start);

        if (previous != start) {
            out += "-";
            out += String(previous);
        }

        if (i < pages.size()) {
            start = pages[i];
            previous = pages[i];
        }
    }

    return out;
}
}

void MifareUltralightReader::classifyVersion(const std::vector<uint8_t>& version, CardInfo& card, int& maxPages) const
{
    if (version.size() < 8) {
        return;
    }

    if (version[2] == 0x03) {
        card.type = CardType::MifareUltralight;
        card.family = CardFamily::Type2;
        card.exactType = "MIFARE Ultralight EV1";

        if (version[6] == 0x0B) {
            maxPages = 20;
        } else if (version[6] == 0x0E) {
            maxPages = 44;
        }
    }

    if (version[2] == 0x04) {
        if (version[6] == 0x0F) {
            card.type = CardType::Ntag213;
            card.exactType = "NTAG213";
            maxPages = 45;
        } else if (version[6] == 0x11) {
            card.type = CardType::Ntag215;
            card.exactType = "NTAG215";
            maxPages = 135;
        } else if (version[6] == 0x13) {
            card.type = CardType::Ntag216;
            card.exactType = "NTAG216";
            maxPages = 231;
        }

        card.family = CardFamily::Type2;
    }
}

void MifareUltralightReader::enrichType2Model(NfcDump& dump) const
{
    dump.card.family = CardFamily::Type2;
    dump.familyName = cardFamilyName(CardFamily::Type2);
    dump.exactType = dump.card.exactType.length() > 0 ? dump.card.exactType : cardTypeName(dump.card.type);
    dump.product = dump.exactType;
    dump.protocol = "ISO14443A Type 2";
    dump.storageSize = dump.unitsTotal * 4;
    dump.storage = String(dump.storageSize) + " bytes";
    dump.pagesTotal = dump.unitsTotal;
    dump.pagesRead = dump.unitsRead;
    dump.readMethod = readMethodName(ReadMethod::Type2RawCrc);

    if (dump.data.size() >= 0x11 * 4) {
        dump.auth0 = bytesToHex(&dump.data[0x10 * 4 + 3], 1, false);
    }

    if (dump.data.size() >= 0x12 * 4) {
        dump.access = bytesToHex(&dump.data[0x11 * 4 + 1], 1, false);
    }

    if (dump.data.size() >= 3 * 4) {
        dump.lockBytes = bytesToHex(&dump.data[2 * 4 + 2], 2, true);
    }

    if (dump.data.size() >= 4 * 4) {
        dump.otpBytes = bytesToHex(&dump.data[3 * 4], 4, true);
    }

    int auth0Page = -1;

    if (dump.auth0.length() > 0) {
        auth0Page = strtol(dump.auth0.c_str(), nullptr, 16);
    }

    dump.protectedUnits = 0;
    dump.unknownUnits = 0;
    dump.pagesProtected = 0;
    dump.pagesUnknown = 0;
    dump.protectedRanges.clear();
    dump.unknownRanges.clear();
    dump.readableRanges.clear();

    std::vector<uint8_t> readable;
    std::vector<uint8_t> protectedPages;
    std::vector<uint8_t> unknownPages;

    for (int page = 0; page < dump.unitsTotal; page++) {
        bool missing = false;

        for (uint8_t missingPage : dump.missingUnits) {
            if (missingPage == page) {
                missing = true;
                break;
            }
        }

        if (!missing) {
            readable.push_back(page);
        } else if (auth0Page >= 0 && page >= auth0Page) {
            protectedPages.push_back(page);
        } else {
            unknownPages.push_back(page);
        }
    }

    dump.protectedUnits = protectedPages.size();
    dump.unknownUnits = unknownPages.size();
    dump.pagesProtected = dump.protectedUnits;
    dump.pagesUnknown = dump.unknownUnits;
    dump.readableRanges.push_back(pageRanges(readable));
    dump.protectedRanges.push_back(pageRanges(protectedPages));
    dump.unknownRanges.push_back(pageRanges(unknownPages));

    if (dump.unitsRead == 0) {
        dump.statusSummary = "Card detected, memory unreadable";
        dump.cloneSummary = "No memory dump";
        dump.actionCapabilities = "Save info, Diagnostics";
    } else if (dump.status == DumpStatus::Full) {
        dump.statusSummary = "Readable";
        dump.cloneSummary = "Full dump";
        dump.actionCapabilities = "Save: yes, Emulate full: yes, Diagnostics: yes";
    } else if (dump.protectedUnits > 0) {
        dump.statusSummary = "Readable user memory, protected area";
        dump.cloneSummary = "Partial only";
        dump.actionCapabilities = "Save: yes, Emulate full: no, Diagnostics: yes";
    } else {
        dump.statusSummary = "Readable with unknown gaps";
        dump.cloneSummary = "Partial only";
        dump.actionCapabilities = "Save: yes, Emulate full: no, Diagnostics: yes";
    }
}

void MifareUltralightReader::enrichReport(NfcDump& dump, const std::vector<uint8_t>& version, bool rawReadOk, bool rawFastReadWorked, bool fallbackUsed, int fallbackRecovered) const
{
    dump.vendor = version.empty() ? "unknown" : "NXP";
    dump.product = dump.exactType.length() > 0 ? dump.exactType : cardTypeName(dump.card.type);
    dump.protocol = "ISO14443A Type 2";
    dump.storage = String(dump.unitsTotal * 4) + " bytes";
    dump.getVersion = version.empty() ? "ERR" : bytesToHex(version.data(), version.size(), false);

    dump.reportLines.push_back("Type: " + dump.product);
    dump.reportLines.push_back("Family: " + dump.familyName);
    dump.reportLines.push_back("Summary: " + dump.statusSummary);
    dump.reportLines.push_back("Clone: " + dump.cloneSummary);
    dump.reportLines.push_back("Vendor: " + dump.vendor);
    dump.reportLines.push_back("Storage: " + dump.storage);
    dump.reportLines.push_back("Protocol: " + dump.protocol);
    dump.reportLines.push_back("GET_VERSION: " + dump.getVersion);
    dump.reportLines.push_back(String("Type2 raw READ: ") + (rawReadOk ? "OK" : "ERR"));
    dump.reportLines.push_back(String("Type2 raw FAST_READ: ") + (rawFastReadWorked ? "OK" : "ERR/unused"));
    dump.reportLines.push_back(String("Fallback IDX used: ") + (fallbackUsed ? "yes" : "no"));
    dump.reportLines.push_back("READ windows: " + String(fallbackRecovered));
    dump.reportLines.push_back("Pages read: " + String(dump.unitsRead) + "/" + String(dump.unitsTotal));
    dump.reportLines.push_back("Protected pages: " + String(dump.protectedUnits));
    dump.reportLines.push_back("Unknown pages: " + String(dump.unknownUnits));
    dump.reportLines.push_back("Readable: " + (dump.readableRanges.empty() ? String("-") : dump.readableRanges[0]));
    dump.reportLines.push_back("Protected: " + (dump.protectedRanges.empty() ? String("-") : dump.protectedRanges[0]));
    dump.reportLines.push_back("Unknown: " + (dump.unknownRanges.empty() ? String("-") : dump.unknownRanges[0]));
    dump.reportLines.push_back("LOCK: " + (dump.lockBytes.length() > 0 ? dump.lockBytes : "unknown"));
    dump.reportLines.push_back("OTP: " + (dump.otpBytes.length() > 0 ? dump.otpBytes : "unknown"));
    dump.reportLines.push_back("AUTH0: " + (dump.auth0 == "FF" ? String("FF no read protection configured") : (dump.auth0.length() > 0 ? dump.auth0 : "unknown")));
    dump.reportLines.push_back("ACCESS: " + (dump.access.length() > 0 ? dump.access : "unknown"));
}
