// Point d'entrée de la simulation desktop SDL.
//
// Reproduit le câblage et la boucle principale de src/main.cpp, mais alimente
// KeyInput depuis le clavier SDL au lieu du clavier M5Cardputer.
//
// L'app NFC est volontairement absente : elle dépend de SD, SPI, Serial1, BLE
// et de la pile USB host, qui n'existent pas sur desktop.

#include <M5Cardputer.h>
#include <M5GFX.h>

#include "apps/charge/ChargeModeApp.h"
#include "apps/home/HomeApp.h"
#include "apps/placeholder/PlaceholderApp.h"
#include "apps/settings/SettingsApp.h"
#include "core/AppDescriptor.h"
#include "core/AppManager.h"
#include "core/PowerManager.h"
#include "core/SettingsManager.h"

#if defined(SDL_h_)

namespace {

void launchSettings();
void launchChargeMode();
void launchPlaceholder();

AppManager appManager;
SettingsManager settingsManager;
PowerManager powerManager;

PlaceholderApp pn532App("PN532Killer", "NFC toolkit", powerManager);
PlaceholderApp meshtasticApp("Meshtastic", "LoRa mesh", powerManager);
PlaceholderApp wifiApp("WiFi", "Wireless tools", powerManager);
PlaceholderApp bleApp("BLE", "Bluetooth tools", powerManager);
PlaceholderApp subGhzApp("SubGHz", "CC1101 tools", powerManager);
PlaceholderApp nrf24App("NRF24", "2.4GHz tools", powerManager);
PlaceholderApp filesApp("Files", "SD browser", powerManager);
PlaceholderApp badUsbApp("BadUSB", "Payload runner", powerManager);
PlaceholderApp voiceApp("Voice", "Memo recorder", powerManager);
PlaceholderApp vitalsApp("Vitals", "Service checks", powerManager);
ChargeModeApp chargeModeApp(powerManager, settingsManager);
SettingsApp settingsApp(settingsManager, powerManager, launchChargeMode);

PlaceholderApp* pendingPlaceholder = nullptr;

void openPlaceholder(PlaceholderApp& app)
{
    pendingPlaceholder = &app;
    appManager.setApp(app);
}

void launchPn532() { openPlaceholder(pn532App); }
void launchMeshtastic() { openPlaceholder(meshtasticApp); }
void launchWifi() { openPlaceholder(wifiApp); }
void launchBle() { openPlaceholder(bleApp); }
void launchSubGhz() { openPlaceholder(subGhzApp); }
void launchNrf24() { openPlaceholder(nrf24App); }
void launchFiles() { openPlaceholder(filesApp); }
void launchBadUsb() { openPlaceholder(badUsbApp); }
void launchVoice() { openPlaceholder(voiceApp); }
void launchVitals() { openPlaceholder(vitalsApp); }

AppDescriptor launcherApps[] = {
    {"PN532Killer", AppIcon::Pn532Killer, launchPn532},
    {"Meshtastic", AppIcon::Meshtastic, launchMeshtastic},
    {"WiFi", AppIcon::Wifi, launchWifi},
    {"BLE", AppIcon::Ble, launchBle},
    {"SubGHz", AppIcon::SubGhz, launchSubGhz},
    {"NRF24", AppIcon::Nrf24, launchNrf24},
    {"Files", AppIcon::Files, launchFiles},
    {"BadUSB", AppIcon::BadUsb, launchBadUsb},
    {"Voice", AppIcon::Voice, launchVoice},
    {"Vitals", AppIcon::Vitals, launchVitals},
    {"Settings", AppIcon::Settings, launchSettings},
    {"Charge Mode", AppIcon::Charge, launchChargeMode},
};

constexpr int launcherAppCount = sizeof(launcherApps) / sizeof(launcherApps[0]);
HomeApp homeApp(powerManager, launcherApps, launcherAppCount);

void launchSettings() { appManager.setApp(settingsApp); }
void launchChargeMode() { appManager.setApp(chargeModeApp); }
void launchPlaceholder() {}

// Table de correspondance scancode SDL -> caractere Cardputer.
// Les fleches physiques du Mac sont acceptees en plus des touches
// serigraphiees ; . , / du Cardputer, pour le confort de developpement.
struct KeyMap {
    SDL_Scancode scancode;
    char character;
};

constexpr KeyMap printableKeys[] = {
    {SDL_SCANCODE_A, 'a'}, {SDL_SCANCODE_B, 'b'}, {SDL_SCANCODE_C, 'c'},
    {SDL_SCANCODE_D, 'd'}, {SDL_SCANCODE_E, 'e'}, {SDL_SCANCODE_F, 'f'},
    {SDL_SCANCODE_G, 'g'}, {SDL_SCANCODE_H, 'h'}, {SDL_SCANCODE_I, 'i'},
    {SDL_SCANCODE_J, 'j'}, {SDL_SCANCODE_K, 'k'}, {SDL_SCANCODE_L, 'l'},
    {SDL_SCANCODE_M, 'm'}, {SDL_SCANCODE_N, 'n'}, {SDL_SCANCODE_O, 'o'},
    {SDL_SCANCODE_P, 'p'}, {SDL_SCANCODE_Q, 'q'}, {SDL_SCANCODE_R, 'r'},
    {SDL_SCANCODE_S, 's'}, {SDL_SCANCODE_T, 't'}, {SDL_SCANCODE_U, 'u'},
    {SDL_SCANCODE_V, 'v'}, {SDL_SCANCODE_W, 'w'}, {SDL_SCANCODE_X, 'x'},
    {SDL_SCANCODE_Y, 'y'}, {SDL_SCANCODE_Z, 'z'},
    {SDL_SCANCODE_0, '0'}, {SDL_SCANCODE_1, '1'}, {SDL_SCANCODE_2, '2'},
    {SDL_SCANCODE_3, '3'}, {SDL_SCANCODE_4, '4'}, {SDL_SCANCODE_5, '5'},
    {SDL_SCANCODE_6, '6'}, {SDL_SCANCODE_7, '7'}, {SDL_SCANCODE_8, '8'},
    {SDL_SCANCODE_9, '9'},
    {SDL_SCANCODE_SPACE, ' '},
    {SDL_SCANCODE_MINUS, '-'},
    {SDL_SCANCODE_PERIOD, '.'},
    {SDL_SCANCODE_COMMA, ','},
    {SDL_SCANCODE_SLASH, '/'},
    {SDL_SCANCODE_SEMICOLON, ';'},
    {SDL_SCANCODE_GRAVE, '`'},
};

constexpr int printableKeyCount = sizeof(printableKeys) / sizeof(printableKeys[0]);

Uint8 previousKeys[SDL_NUM_SCANCODES] = {};

bool pressedNow(const Uint8* state, SDL_Scancode code)
{
    return state[code] != 0 && previousKeys[code] == 0;
}

// Reproduit readKeyInput() de src/main.cpp : les caracteres nourrissent
// characters, et les directions sont deduites des caracteres serigraphies.
bool readKeyInput(KeyInput& input)
{
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    bool changed = false;

    for (int i = 0; i < printableKeyCount; ++i) {
        if (pressedNow(state, printableKeys[i].scancode)) {
            input.characters += printableKeys[i].character;
            changed = true;
        }
    }

    const bool escPressed = pressedNow(state, SDL_SCANCODE_ESCAPE);
    const bool tabPressed = pressedNow(state, SDL_SCANCODE_TAB);
    const bool backspacePressed = pressedNow(state, SDL_SCANCODE_BACKSPACE);
    const bool deletePressed = pressedNow(state, SDL_SCANCODE_DELETE);
    const bool enterPressed = pressedNow(state, SDL_SCANCODE_RETURN);
    const bool upPressed = pressedNow(state, SDL_SCANCODE_UP);
    const bool downPressed = pressedNow(state, SDL_SCANCODE_DOWN);
    const bool leftPressed = pressedNow(state, SDL_SCANCODE_LEFT);
    const bool rightPressed = pressedNow(state, SDL_SCANCODE_RIGHT);

    changed = changed || escPressed || tabPressed || backspacePressed
        || deletePressed || enterPressed || upPressed || downPressed
        || leftPressed || rightPressed;

    SDL_memcpy(previousKeys, state, SDL_NUM_SCANCODES);

    if (!changed) {
        return false;
    }

    input.esc = escPressed || input.characters.indexOf('`') >= 0;
    input.tab = tabPressed;
    input.backspace = backspacePressed;
    input.del = deletePressed;
    input.enter = enterPressed;
    input.up = upPressed || input.characters.indexOf(';') >= 0;
    input.down = downPressed || input.characters.indexOf('.') >= 0;
    input.left = leftPressed || input.characters.indexOf(',') >= 0;
    input.right = rightPressed || input.characters.indexOf('/') >= 0;

    return true;
}

bool handleGlobalNavigation(const KeyInput& input)
{
    if (!input.esc) {
        return false;
    }

    if (appManager.isCurrentApp(homeApp)) {
        launchSettings();
    } else {
        appManager.setApp(homeApp);
    }

    return true;
}

} // namespace

void setup()
{
    M5Cardputer.begin();
    settingsManager.begin();
    M5Cardputer.Display.setBrightness(settingsManager.hardwareBrightness());
    powerManager.update();
    appManager.setApp(homeApp);
}

void loop()
{
    M5Cardputer.update();

    KeyInput input;
    if (readKeyInput(input)) {
        if (!handleGlobalNavigation(input)) {
            appManager.onKey(input);
        }
    }

    appManager.update();
    delay(10);
}

__attribute__((weak)) int user_func(bool* running)
{
    setup();
    do {
        loop();
    } while (*running);
    return 0;
}

int main(int, char**)
{
    return lgfx::Panel_sdl::main(user_func, 128);
}

#endif // SDL_h_
