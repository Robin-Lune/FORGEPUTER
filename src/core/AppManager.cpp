#include "AppManager.h"

void AppManager::setApp(App& app)
{
    if (currentApp_ != nullptr) {
        currentApp_->close();
    }

    currentApp_ = &app;
    currentApp_->init();
    currentApp_->draw();
}

bool AppManager::isCurrentApp(const App& app) const
{
    return currentApp_ == &app;
}

void AppManager::update()
{
    if (currentApp_ != nullptr) {
        currentApp_->update();
    }
}

void AppManager::draw()
{
    if (currentApp_ != nullptr) {
        currentApp_->draw();
    }
}

void AppManager::onKey(const KeyInput& input)
{
    if (currentApp_ != nullptr) {
        currentApp_->onKey(input);
    }
}
