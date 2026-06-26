#pragma once

#include "../../core/App.h"

class HomeApp : public App {
public:
    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    String inputLine_ = "> ";

    void drawHeader();
    void drawStatus(const char* message);
    void drawInputLine();
};
