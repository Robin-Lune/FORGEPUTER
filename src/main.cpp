#include <M5Cardputer.h>

#include "apps/home/HomeApp.h"
#include "core/AppManager.h"

namespace {
AppManager appManager;
HomeApp homeApp;

bool readKeyInput(KeyInput& input)
{
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return false;
    }

    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    for (auto key : keys.word) {
        input.characters += key;
    }

    input.backspace = keys.backspace;
    input.del = keys.del;
    input.enter = keys.enter;

    return true;
}
}

void setup()
{
    auto cfg = M5.config();

    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    delay(100);

    appManager.setApp(homeApp);
    Serial.println("Forgeputer booted");
}

void loop()
{
    M5Cardputer.update();

    KeyInput input;
    if (readKeyInput(input)) {
        appManager.onKey(input);
    }

    appManager.update();
    delay(10);
}
