#pragma once

#include "App.h"

class AppManager {
public:
    void setApp(App& app);
    bool isCurrentApp(const App& app) const;
    void update();
    void draw();
    void onKey(const KeyInput& input);

private:
    App* currentApp_ = nullptr;
};
