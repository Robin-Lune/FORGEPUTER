#include "ReadAutoController.h"

#include <cstring>

ReadAutoController::ReadAutoController(Pn532Client& pn532, KeyStore& keys)
    : pn532_(pn532),
      keys_(keys),
      ultralightReader_(pn532)
{
}

bool ReadAutoController::scan(CardInfo& info)
{
    if (!pn532_.setNormalMode() || !pn532_.scanIso14443A(info)) {
        setError(pn532_.lastError());
        return false;
    }

    return true;
}

bool ReadAutoController::readAuto(NfcDump& dump)
{
    CardInfo info;

    if (!scan(info)) {
        return false;
    }

    if (isMifareClassic(info.type)) {
        return readMifareClassic(info, dump);
    }

    if (info.type == CardType::MifareUltralight) {
        return ultralightReader_.read(info, dump);
    }

    dump = NfcDump();
    dump.card = info;
    dump.status = DumpStatus::InfoOnly;
    dump.unitsTotal = 0;
    dump.unitsRead = 0;
    return true;
}

bool ReadAutoController::retryMissing(NfcDump& dump)
{
    if (!isMifareClassic(dump.card.type) || dump.missingUnits.empty()) {
        setError("No MFC missing sectors");
        return false;
    }

    CardInfo info;

    if (!scan(info)) {
        return false;
    }

    if (info.uid != dump.card.uid || info.type != dump.card.type) {
        setError("Different card");
        return false;
    }

    if (dump.data.empty()) {
        dump.data.assign(mifareClassicBlockCount(dump.card.type) * 16, 0);
    }

    std::vector<uint8_t> stillMissing;
    const int totalMissing = dump.missingUnits.size();
    emitProgress("Retry MFC", "Missing sectors", 0, totalMissing, false, true);

    for (int i = 0; i < totalMissing; i++) {
        const uint8_t sector = dump.missingUnits[i];
        const uint8_t firstBlock = firstBlockForSector(sector);
        bool authenticated = false;
        emitProgress("Retry MFC", "Sector " + String(sector) + " auth", i, totalMissing, false, true);

        for (const auto& key : keys_.keys()) {
            if (pn532_.mifareAuthenticate(info, firstBlock, key.data(), true) || pn532_.mifareAuthenticate(info, firstBlock, key.data(), false)) {
                authenticated = true;
                break;
            }
        }

        if (!authenticated) {
            stillMissing.push_back(sector);
            emitProgress("Retry MFC", "Sector " + String(sector) + " LOCKED", i + 1, totalMissing, true, false);
            continue;
        }

        bool sectorRead = true;
        const uint8_t blocks = blockCountForSector(sector);

        for (uint8_t offset = 0; offset < blocks; offset++) {
            uint8_t blockData[16];
            const uint8_t block = firstBlock + offset;

            if (!pn532_.mifareReadBlock(block, blockData)) {
                sectorRead = false;
                break;
            }

            memcpy(&dump.data[block * 16], blockData, 16);
        }

        if (sectorRead) {
            dump.unitsRead++;
            emitProgress("Retry MFC", "Sector " + String(sector) + " OK", i + 1, totalMissing, true, true);
        } else {
            stillMissing.push_back(sector);
            emitProgress("Retry MFC", "Sector " + String(sector) + " READ ERR", i + 1, totalMissing, true, false);
        }
    }

    dump.missingUnits = stillMissing;
    dump.card.isPartial = !dump.missingUnits.empty();
    dump.status = dump.missingUnits.empty() ? DumpStatus::Full : DumpStatus::Partial;
    return true;
}

String ReadAutoController::lastError() const
{
    return lastError_;
}

void ReadAutoController::setProgressCallback(void* context, NfcReadProgressCallback callback)
{
    progressContext_ = context;
    progressCallback_ = callback;
    ultralightReader_.setProgressCallback(context, callback);
}

bool ReadAutoController::readMifareClassic(const CardInfo& info, NfcDump& dump)
{
    dump = NfcDump();
    dump.card = info;
    dump.unitsTotal = mifareClassicSectorCount(info.type);
    dump.data.assign(mifareClassicBlockCount(info.type) * 16, 0);
    emitProgress("Reading MFC", info.uid, 0, dump.unitsTotal, false, true);

    for (int sector = 0; sector < dump.unitsTotal; sector++) {
        const uint8_t firstBlock = firstBlockForSector(sector);
        bool authenticated = false;
        emitProgress("Reading MFC", "Sector " + String(sector) + " auth", sector, dump.unitsTotal, false, true);

        for (const auto& key : keys_.keys()) {
            if (pn532_.mifareAuthenticate(info, firstBlock, key.data(), true) || pn532_.mifareAuthenticate(info, firstBlock, key.data(), false)) {
                authenticated = true;
                break;
            }
        }

        if (!authenticated) {
            dump.missingUnits.push_back(sector);
            emitProgress("Reading MFC", "Sector " + String(sector) + " LOCKED", sector + 1, dump.unitsTotal, true, false);
            continue;
        }

        bool sectorRead = true;
        const uint8_t blocks = blockCountForSector(sector);

        for (uint8_t offset = 0; offset < blocks; offset++) {
            uint8_t blockData[16];
            const uint8_t block = firstBlock + offset;

            if (!pn532_.mifareReadBlock(block, blockData)) {
                sectorRead = false;
                break;
            }

            memcpy(&dump.data[block * 16], blockData, 16);
        }

        if (sectorRead) {
            dump.unitsRead++;
            emitProgress("Reading MFC", "Sector " + String(sector) + " OK", sector + 1, dump.unitsTotal, true, true);
        } else {
            dump.missingUnits.push_back(sector);
            emitProgress("Reading MFC", "Sector " + String(sector) + " READ ERR", sector + 1, dump.unitsTotal, true, false);
        }
    }

    dump.card.isPartial = dump.unitsRead < dump.unitsTotal;
    dump.status = dump.unitsRead == dump.unitsTotal ? DumpStatus::Full : DumpStatus::Partial;
    return dump.unitsRead > 0;
}

bool ReadAutoController::readNtag(const CardInfo& info, NfcDump& dump)
{
    dump = NfcDump();
    dump.card = info;
    constexpr int maxPages = 45;
    dump.unitsTotal = maxPages;
    std::vector<uint8_t> pagesData(maxPages * 4, 0);
    std::vector<bool> pageRead(maxPages, false);
    emitProgress("Reading NTAG", info.uid, 0, dump.unitsTotal, false, true);

    for (uint8_t page = 0; page < maxPages; page++) {
        uint8_t pages[16];
        emitProgress("Reading NTAG", "Window " + String(page) + "-" + String(min(static_cast<int>(page + 3), maxPages - 1)), page, maxPages, false, true);

        if (!pn532_.ntagReadPages(page, pages)) {
            std::vector<uint8_t> fastData;

            if (pn532_.ntagFastRead(page, min(static_cast<int>(page + 3), maxPages - 1), fastData) && fastData.size() >= 16) {
                memcpy(pages, fastData.data(), 16);
            } else {
                emitProgress("Reading NTAG", "Window " + String(page) + " ERR", page, maxPages, true, false);
                continue;
            }
        }

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
    return dump.unitsRead > 0;
}

uint8_t ReadAutoController::firstBlockForSector(int sector) const
{
    if (sector < 32) {
        return sector * 4;
    }

    return 128 + ((sector - 32) * 16);
}

uint8_t ReadAutoController::blockCountForSector(int sector) const
{
    return sector < 32 ? 4 : 16;
}

bool ReadAutoController::isMifareClassic(CardType type) const
{
    return type == CardType::MifareClassicMini || type == CardType::MifareClassic1K || type == CardType::MifareClassic4K;
}

void ReadAutoController::emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok)
{
    if (progressCallback_ == nullptr) {
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

void ReadAutoController::setError(const String& error)
{
    lastError_ = error;
}
