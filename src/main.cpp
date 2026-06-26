#include <M5Cardputer.h>

namespace {
String inputLine = "> ";

void drawHeader()
{
    auto& display = M5Cardputer.Display;

    display.setRotation(1);
    display.fillScreen(BLACK);
    display.setTextColor(GREEN);
    display.setTextSize(2);
    display.setCursor(8, 8);
    display.print("Forgeputer");

    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(8, 34);
    display.print("Cardputer ADV firmware");
}

void drawStatus(const char* message)
{
    auto& display = M5Cardputer.Display;

    display.fillRect(8, 58, display.width() - 16, 24, BLACK);
    display.setTextColor(GREEN);
    display.setTextSize(1);
    display.setCursor(8, 58);
    display.print(message);
}

void drawInputLine()
{
    auto& display = M5Cardputer.Display;
    const int y = display.height() - 24;

    display.fillRect(0, y, display.width(), 24, BLACK);
    display.drawFastHLine(0, y, display.width(), GREEN);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(8, y + 8);
    display.print(inputLine);
}

void drawHome()
{
    drawHeader();
    drawStatus("Ready");
    drawInputLine();
}

void handleKeyboard()
{
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    for (auto key : keys.word) {
        inputLine += key;
    }

    if (keys.del && inputLine.length() > 2) {
        inputLine.remove(inputLine.length() - 1);
    }

    if (keys.enter) {
        Serial.print("Input: ");
        Serial.println(inputLine.substring(2));
        inputLine = "> ";
        drawStatus("Input received");
    }

    drawInputLine();
}
}

void setup()
{
    auto cfg = M5.config();

    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    delay(100);

    drawHome();
    Serial.println("Forgeputer booted");
}

void loop()
{
    M5Cardputer.update();
    handleKeyboard();
    delay(10);
}
