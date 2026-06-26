#include <M5Cardputer.h>

#include "apps/home/HomeApp.h"
#include "apps/settings/SettingsApp.h"
#include "core/AppManager.h"
#include "core/SettingsManager.h"

namespace {
AppManager appManager;
SettingsManager settingsManager;
HomeApp homeApp;
SettingsApp settingsApp(settingsManager);

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
        appManager.setApp(settingsApp);
    } else {
        appManager.setApp(homeApp);
    }

    return true;
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
