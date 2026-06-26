#pragma once

#include "App.h"

class AppManager {
public:
    void setApp(App& app);
    void update();
    void draw();
    void onKey(const KeyInput& input);

private:
    App* currentApp_ = nullptr;
};
