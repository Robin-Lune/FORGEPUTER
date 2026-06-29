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
    int maxPages = 45;
    bool rawFastReadWorked = false;
    bool fallbackUsed = false;
    int fallbackRecovered = 0;

    emitProgress("Reading MFU", card.uid, 0, maxPages, false, true);

    Pn532NfcAResult read04 = pn532_.type2TransceiveRaw({0x30, 0x04}, 16, 1000);

    if (!read04.accepted) {
        dump.unitsTotal = maxPages;
        dump.unitsRead = 0;
        dump.unknownUnits = maxPages;
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

    for (uint8_t page = 0; page < maxPages; page++) {
        std::vector<uint8_t> window;
        String source;
        emitProgress("Reading MFU", "Page " + String(page), page, maxPages, false, true);

        if (!tryReadWindow(page, min(static_cast<int>(page + 3), maxPages - 1), window, source)) {
            emitProgress("Reading MFU", "Page " + String(page) + " ERR", page, maxPages, true, false);
            continue;
        }

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

void MifareUltralightReader::classifyVersion(const std::vector<uint8_t>& version, CardInfo& card, int& maxPages) const
{
    if (version.size() < 8) {
        return;
    }

    if (version[2] == 0x03) {
        card.type = CardType::MifareUltralight;

        if (version[6] == 0x0B) {
            maxPages = 20;
        } else if (version[6] == 0x0E) {
            maxPages = 44;
        }
    }

    if (version[2] == 0x04) {
        if (version[6] == 0x0F) {
            card.type = CardType::Ntag213;
            maxPages = 45;
        } else if (version[6] == 0x11) {
            card.type = CardType::Ntag215;
            maxPages = 135;
        } else if (version[6] == 0x13) {
            card.type = CardType::Ntag216;
            maxPages = 231;
        }
    }
}

void MifareUltralightReader::enrichReport(NfcDump& dump, const std::vector<uint8_t>& version, bool rawReadOk, bool rawFastReadWorked, bool fallbackUsed, int fallbackRecovered) const
{
    dump.vendor = version.empty() ? "unknown" : "NXP";
    dump.product = cardTypeName(dump.card.type);
    dump.protocol = "ISO14443A Type 2";
    dump.storage = String(dump.unitsTotal * 4) + " bytes";
    dump.getVersion = version.empty() ? "ERR" : bytesToHex(version.data(), version.size(), false);

    if (dump.data.size() >= 0x11 * 4) {
        dump.auth0 = bytesToHex(&dump.data[0x10 * 4 + 3], 1, false);
    }

    if (dump.data.size() >= 0x12 * 4) {
        dump.access = bytesToHex(&dump.data[0x11 * 4 + 1], 1, false);
    }

    dump.reportLines.push_back("Type: " + dump.product);
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
    dump.reportLines.push_back("AUTH0: " + (dump.auth0.length() > 0 ? dump.auth0 : "unknown"));
    dump.reportLines.push_back("ACCESS: " + (dump.access.length() > 0 ? dump.access : "unknown"));
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
