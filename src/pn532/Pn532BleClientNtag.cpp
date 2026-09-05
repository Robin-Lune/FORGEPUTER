// Pn532BleClient - MFU/NTAG read over BLE and its capability report.

#include "Pn532BleClient.h"

#include <cstdlib>
#include <cstring>

namespace {
String diagPageRanges(const std::vector<uint8_t>& pages)
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

bool Pn532BleClient::readNtag(const PN532_BLE::Iso14aTagInfo& tag, const CardInfo& info, NfcDump& dump)
{
    dump = NfcDump();
    dump.card = info;
    dump.card.family = CardFamily::Type2;
    dump.familyName = cardFamilyName(CardFamily::Type2);
    int maxPages = 45;
    const Pn532NfcAResult version = type2TransceiveRaw({0x60}, 8, 1000);

    if (version.accepted && version.payload.size() >= 8) {
        if (version.payload[2] == 0x03) {
            dump.card.type = CardType::MifareUltralight;
            dump.card.exactType = "MIFARE Ultralight EV1";

            if (version.payload[6] == 0x0B) {
                maxPages = 20;
            } else if (version.payload[6] == 0x0E) {
                maxPages = 44;
            }
        } else if (version.payload[2] == 0x04) {
            if (version.payload[6] == 0x0F) {
                dump.card.type = CardType::Ntag213;
                dump.card.exactType = "NTAG213";
                maxPages = 45;
            } else if (version.payload[6] == 0x11) {
                dump.card.type = CardType::Ntag215;
                dump.card.exactType = "NTAG215";
                maxPages = 135;
            } else if (version.payload[6] == 0x13) {
                dump.card.type = CardType::Ntag216;
                dump.card.exactType = "NTAG216";
                maxPages = 231;
            }
        }
    }

    dump.exactType = dump.card.exactType.length() > 0 ? dump.card.exactType : cardTypeName(dump.card.type);
    dump.product = dump.exactType;
    dump.protocol = "ISO14443A Type 2";
    dump.storageSize = maxPages * 4;
    dump.storage = String(dump.storageSize) + " bytes";
    dump.readMethod = readMethodName(ReadMethod::Type2RawCrc);
    dump.unitsTotal = maxPages;
    bool rawFastReadWorked = false;
    bool fallbackUsed = false;

    Pn532NfcAResult read04 = type2TransceiveRaw({0x30, 0x04}, 16, 1000);

    if (!read04.accepted) {
        dump.unitsRead = 0;
        dump.pagesTotal = maxPages;
        dump.pagesRead = 0;
        dump.pagesUnknown = maxPages;
        dump.unknownUnits = maxPages;
        dump.statusSummary = "Memory read failed";
        dump.cloneSummary = "No memory dump";
        dump.actionCapabilities = "Save info, Diagnostics";
        dump.card.isPartial = true;
        dump.status = DumpStatus::Partial;
        dump.reportLines.push_back("Type2 raw READ: ERR");
        dump.reportLines.push_back("Type2 raw FAST_READ: not run");
        dump.reportLines.push_back("Fallback IDX used: no");
        dump.reportLines.push_back("Pages read: 0/" + String(maxPages));
        dump.reportLines.push_back("Protected pages: 0");
        dump.reportLines.push_back("Unknown pages: " + String(maxPages));
        dump.reportLines.push_back("Type 2 raw read failed");
        setError("Type 2 raw read failed");
        emitProgress("Reading NTAG", "Type 2 raw read failed", 0, maxPages, true, false);
        return false;
    }

    std::vector<uint8_t> pagesData(maxPages * 4, 0);
    std::vector<bool> pageRead(maxPages, false);
    int consecutiveErrors = 0;
    emitProgress("Reading NTAG", info.uid, 0, dump.unitsTotal, false, true);

    for (uint8_t page = 0; page < maxPages; page++) {
        emitProgress("Reading NTAG", "Window " + String(page) + "-" + String(min(static_cast<int>(page + 3), maxPages - 1)), page, maxPages, false, true);
        std::vector<uint8_t> pages;
        const Pn532NfcAResult rawRead = type2TransceiveRaw({0x30, page}, 16, 1000);

        if (rawRead.accepted) {
            pages = rawRead.payload;
        } else {
            const Pn532NfcAResult rawFast = type2TransceiveRaw({0x3A, page, static_cast<uint8_t>(min(static_cast<int>(page + 3), maxPages - 1))}, 16, 1500);

            if (rawFast.accepted) {
                pages = rawFast.payload;
                rawFastReadWorked = true;
            }
        }

        if (pages.empty()) {
            fallbackUsed = true;
            pages = ble_->mfRdbl(page);

            if (pages.size() >= 17 && pages[0] == 0x00) {
                pages.erase(pages.begin());
            }
        }

        if (pages.size() < 16) {
            emitProgress("Reading NTAG", "Window " + String(page) + " ERR", page, maxPages, true, false);

            if (page >= 4) {
                consecutiveErrors++;

                if (consecutiveErrors >= 4) {
                    emitProgress("Reading NTAG", "Stop after errors", page, maxPages, true, false);
                    break;
                }
            }

            continue;
        }

        consecutiveErrors = 0;

        int newPages = 0;

        for (int offset = 0; offset < 4 && page + offset < maxPages; offset++) {
            const int pageIndex = page + offset;

            if (!pageRead[pageIndex]) {
                memcpy(&pagesData[pageIndex * 4], &pages[offset * 4], 4);
                pageRead[pageIndex] = true;
                newPages++;
            }
        }

        dump.unitsRead += newPages;
        emitProgress("Reading NTAG", "Window " + String(page) + " +" + String(newPages), dump.unitsRead, maxPages, true, true);
    }

    int highestReadPage = -1;

    for (int page = 0; page < maxPages; page++) {
        if (pageRead[page]) {
            highestReadPage = page;
        }
    }

    if (highestReadPage >= 0) {
        dump.unitsTotal = highestReadPage + 1;
        dump.data.assign(pagesData.begin(), pagesData.begin() + (dump.unitsTotal * 4));
    } else {
        dump.unitsTotal = maxPages;
    }

    dump.unitsRead = 0;
    dump.missingUnits.clear();

    for (int page = 0; page < dump.unitsTotal; page++) {
        if (pageRead[page]) {
            dump.unitsRead++;
        } else {
            dump.missingUnits.push_back(page);
        }
    }

    dump.card.isPartial = !dump.missingUnits.empty();
    dump.status = dump.unitsRead > 0 && dump.missingUnits.empty() ? DumpStatus::Full : DumpStatus::Partial;
    dump.unknownUnits = dump.missingUnits.size();
    dump.pagesTotal = dump.unitsTotal;
    dump.pagesRead = dump.unitsRead;
    dump.storageSize = dump.unitsTotal * 4;
    dump.storage = String(dump.storageSize) + " bytes";

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

    int auth0Page = dump.auth0.length() > 0 ? strtol(dump.auth0.c_str(), nullptr, 16) : -1;
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
    dump.readableRanges.push_back(diagPageRanges(readable));
    dump.protectedRanges.push_back(diagPageRanges(protectedPages));
    dump.unknownRanges.push_back(diagPageRanges(unknownPages));
    dump.statusSummary = dump.status == DumpStatus::Full ? "Readable" : (dump.protectedUnits > 0 ? "Readable user memory, protected area" : "Readable with unknown gaps");
    dump.cloneSummary = dump.status == DumpStatus::Full ? "Full dump" : "Partial only";
    dump.actionCapabilities = dump.status == DumpStatus::Full ? "Save: yes, Emulate full: yes, Diagnostics: yes" : "Save: yes, Emulate full: no, Diagnostics: yes";
    dump.reportLines.push_back("Type: " + dump.product);
    dump.reportLines.push_back("Family: " + dump.familyName);
    dump.reportLines.push_back("Summary: " + dump.statusSummary);
    dump.reportLines.push_back("Clone: " + dump.cloneSummary);
    dump.reportLines.push_back("Type2 raw READ: OK");
    dump.reportLines.push_back(String("Type2 raw FAST_READ: ") + (rawFastReadWorked ? "OK" : "ERR/unused"));
    dump.reportLines.push_back(String("Fallback IDX used: ") + (fallbackUsed ? "yes" : "no"));
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
    return dump.unitsRead > 0;
}
