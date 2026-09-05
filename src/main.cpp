#include <M5Cardputer.h>

#include "apps/charge/ChargeModeApp.h"
#include "apps/home/HomeApp.h"
#include "apps/pn532/Pn532KillerApp.h"
#include "apps/placeholder/PlaceholderApp.h"
#include "apps/settings/SettingsApp.h"
#include "core/AppDescriptor.h"
#include "core/AppManager.h"
#include "core/PowerManager.h"
#include "core/SettingsManager.h"

namespace {
void launchSettings();
void launchChargeMode();
void launchPn532Killer();
void launchMeshtastic();
void launchWifi();
void launchBle();
void launchSubGhz();
void launchNrf24();
void launchFiles();
void launchBadUsb();
void launchVoice();
void launchVitals();

AppManager appManager;
SettingsManager settingsManager;
PowerManager powerManager;
Pn532KillerApp pn532KillerApp(powerManager);
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
AppDescriptor launcherApps[] = {
    {"PN532Killer", AppIcon::Pn532Killer, launchPn532Killer},
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

bool readKeyInput(KeyInput& input)
{
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return false;
    }

    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    for (auto key : keys.word) {
        input.characters += key;
    }

    input.esc = keys.esc || input.characters.indexOf('`') >= 0;
    input.tab = keys.tab;
    input.backspace = keys.backspace;
    input.del = keys.del;
    input.enter = keys.enter;
    input.up = keys.up || input.characters.indexOf(';') >= 0;
    input.down = keys.down || input.characters.indexOf('.') >= 0;
    input.left = keys.left || input.characters.indexOf(',') >= 0;
    input.right = keys.right || input.characters.indexOf('/') >= 0;

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

void launchSettings()
{
    appManager.setApp(settingsApp);
}

void launchChargeMode()
{
    appManager.setApp(chargeModeApp);
}

void launchPn532Killer()
{
    appManager.setApp(pn532KillerApp);
}

void launchMeshtastic()
{
    appManager.setApp(meshtasticApp);
}

void launchWifi()
{
    appManager.setApp(wifiApp);
}

void launchBle()
{
    appManager.setApp(bleApp);
}

void launchSubGhz()
{
    appManager.setApp(subGhzApp);
}

void launchNrf24()
{
    appManager.setApp(nrf24App);
}

void launchFiles()
{
    appManager.setApp(filesApp);
}

void launchBadUsb()
{
    appManager.setApp(badUsbApp);
}

void launchVoice()
{
    appManager.setApp(voiceApp);
}

void launchVitals()
{
    appManager.setApp(vitalsApp);
}
}

void setup()
{
    auto cfg = M5.config();

    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    delay(100);

    settingsManager.begin();
    M5Cardputer.Display.setBrightness(settingsManager.hardwareBrightness());
    powerManager.update();
    appManager.setApp(homeApp);
    Serial.println("Forgeputer booted");
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
