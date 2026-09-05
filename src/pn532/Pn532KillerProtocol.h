#pragma once

#include <Arduino.h>

// PN532Killer vendor commands and slot geometry shared by the
// Pn532KillerClient*.cpp translation units.
namespace Pn532Killer {
constexpr uint8_t commandSetWorkMode = 0xAC;
constexpr uint8_t commandReadEmulator = 0x1C;
constexpr uint8_t commandWriteEmulator = 0x1E;
constexpr uint8_t workModePn532 = 0x01;
constexpr uint8_t workModeEmulator = 0x02;
constexpr uint8_t killerTypeMfc1K = 0x01;
constexpr uint8_t killerTypeNtag = 0x02;
constexpr uint8_t killerTypeIso15693 = 0x03;
constexpr uint8_t killerTypeEm4100 = 0x04;
constexpr int mfc1kBlockCount = 64;
constexpr int mfc1kSectorCount = 16;
constexpr int mfcBlockSize = 16;
constexpr int type2PageSize = 4;
constexpr int type2WindowPages = 4;
constexpr int type2WindowSize = type2PageSize * type2WindowPages;
constexpr int iso15693BlockSize = 4;
}
