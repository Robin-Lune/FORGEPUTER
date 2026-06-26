#pragma once

#include "KeyInput.h"

class App {
public:
    virtual ~App() = default;

    virtual void init() = 0;
    virtual void update() = 0;
    virtual void draw() = 0;
    virtual void onKey(const KeyInput& input) = 0;
    virtual void close() = 0;
};
