// MifareUltralightReader - MFU/NTAG page reads with raw and IDX fallbacks.
// Version classification and reporting live in MifareUltralightReport.cpp.

#include "MifareUltralightReader.h"

#include <cstring>

MifareUltralightReader::MifareUltralightReader(Pn532Client& pn532)
    : pn532_(pn532)
{
}

void MifareUltralightReader::setProgressCallback(void* context, NfcReadProgressCallback callback)
{
    progressContext_ = context;
    progressCallback_ = callback;
}

bool MifareUltralightReader::supports(const CardInfo& card) const
{
    return card.type == CardType::MifareUltralight || card.type == CardType::Ntag213 || card.type == CardType::Ntag215 || card.type == CardType::Ntag216;
}

bool MifareUltralightReader::read(const CardInfo& card, NfcDump& dump)
{
    dump = NfcDump();
    dump.card = card;
    dump.card.family = CardFamily::Type2;
    dump.familyName = cardFamilyName(CardFamily::Type2);
    int maxPages = 45;
    bool rawFastReadWorked = false;
    bool fallbackUsed = false;
    int fallbackRecovered = 0;

    emitProgress("Reading MFU", card.uid, 0, maxPages, false, true);

    Pn532NfcAResult read04 = pn532_.type2TransceiveRaw({0x30, 0x04}, 16, 1000);

    if (!read04.accepted) {
        dump.unitsTotal = maxPages;
        dump.unitsRead = 0;
        dump.pagesTotal = maxPages;
        dump.pagesRead = 0;
        dump.pagesUnknown = maxPages;
        dump.unknownUnits = maxPages;
        dump.storageSize = maxPages * 4;
        dump.storage = String(dump.storageSize) + " bytes";
        dump.protocol = "ISO14443A Type 2";
        dump.readMethod = readMethodName(ReadMethod::Type2RawCrc);
        dump.exactType = card.exactType.length() > 0 ? card.exactType : cardTypeName(card.type);
        dump.product = dump.exactType;
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
        emitProgress("Reading MFU", "Type 2 raw read failed", 0, maxPages, true, false);
        return false;
    }

    Pn532NfcAResult versionResult = pn532_.type2TransceiveRaw({0x60}, 8, 1000);

    if (!versionResult.accepted) {
        fallbackUsed = true;
        versionResult = pn532_.nfcATransceive({0x60}, 8, 1000);
    }

    std::vector<uint8_t> version;

    if (versionResult.accepted) {
        version = versionResult.payload;
        classifyVersion(version, dump.card, maxPages);
    }

    dump.unitsTotal = maxPages;
    std::vector<uint8_t> pagesData(maxPages * 4, 0);
    std::vector<bool> pageRead(maxPages, false);
    int consecutiveErrors = 0;

    for (uint8_t page = 0; page < maxPages; page++) {
        std::vector<uint8_t> window;
        String source;
        emitProgress("Reading MFU", "Page " + String(page), page, maxPages, false, true);

        if (!tryReadWindow(page, min(static_cast<int>(page + 3), maxPages - 1), window, source)) {
            emitProgress("Reading MFU", "Page " + String(page) + " ERR", page, maxPages, true, false);

            if (page >= 4) {
                consecutiveErrors++;

                if (consecutiveErrors >= 4) {
                    emitProgress("Reading MFU", "Stop after errors", page, maxPages, true, false);
                    break;
                }
            }

            continue;
        }

        consecutiveErrors = 0;

        if (source == "FAST raw") {
            rawFastReadWorked = true;
        } else if (source.indexOf("idx") >= 0 || source == "READ" || source == "FAST") {
            fallbackUsed = true;
        }

        if (source.startsWith("FAST")) {
            rawFastReadWorked = rawFastReadWorked || source == "FAST raw";
        } else if (source.startsWith("READ")) {
            fallbackRecovered++;
        }

        int newPages = 0;

        for (int offset = 0; offset < 4 && page + offset < maxPages && (offset + 1) * 4 <= static_cast<int>(window.size()); offset++) {
            const int pageIndex = page + offset;

            if (!pageRead[pageIndex]) {
                memcpy(&pagesData[pageIndex * 4], &window[offset * 4], 4);
                pageRead[pageIndex] = true;
                newPages++;
            }
        }

        emitProgress("Reading MFU", source + " +" + String(newPages), page + 1, maxPages, true, true);
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
    }

    for (int page = 0; page < dump.unitsTotal; page++) {
        if (pageRead[page]) {
            dump.unitsRead++;
        } else {
            dump.missingUnits.push_back(page);
        }
    }

    dump.unknownUnits = dump.missingUnits.size();
    dump.card.isPartial = !dump.missingUnits.empty();
    dump.status = dump.unitsRead > 0 && dump.missingUnits.empty() ? DumpStatus::Full : DumpStatus::Partial;
    enrichType2Model(dump);
    enrichReport(dump, version, true, rawFastReadWorked, fallbackUsed, fallbackRecovered);
    return dump.unitsRead > 0;
}

bool MifareUltralightReader::retry(NfcDump& dump)
{
    return read(dump.card, dump);
}

String MifareUltralightReader::lastError() const
{
    return lastError_;
}

bool MifareUltralightReader::tryReadWindow(uint8_t startPage, uint8_t endPage, std::vector<uint8_t>& out, String& source)
{
    out.clear();
    source = "";

    Pn532NfcAResult read = pn532_.type2TransceiveRaw({0x30, startPage}, 16, 1000);

    if (read.accepted) {
        out = read.payload;
        source = "READ raw";
        return true;
    }

    Pn532NfcAResult fast = pn532_.type2TransceiveRaw({0x3A, startPage, endPage}, 4, 1500);

    if (fast.accepted) {
        out = fast.payload;
        source = "FAST raw";
        return true;
    }

    read = pn532_.nfcATransceive({0x30, startPage}, 16, 200);

    if (read.accepted) {
        out = read.payload;
        source = read.kind == Pn532NfcAResponseKind::RawPayload ? "READ idx raw" : "READ";
        return true;
    }

    fast = pn532_.nfcATransceive({0x3A, startPage, endPage}, 4, 300);

    if (fast.accepted) {
        out = fast.payload;
        source = fast.kind == Pn532NfcAResponseKind::RawPayload ? "FAST idx raw" : "FAST";
        return true;
    }

    setError(read.error.length() > 0 ? read.error : fast.error);
    return false;
}

void MifareUltralightReader::emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok)
{
    if (!progressCallback_) {
        return;
    }

    NfcReadProgress progress;
    progress.title = title;
    progress.detail = detail;
    progress.current = current;
    progress.total = total;
    progress.done = done;
    progress.ok = ok;
    progressCallback_(progressContext_, progress);
}

void MifareUltralightReader::setError(const String& error)
{
    lastError_ = error;
}
