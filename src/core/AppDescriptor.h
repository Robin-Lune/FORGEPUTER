#pragma once

enum class AppIcon {
    Pn532Killer,
    Meshtastic,
    Wifi,
    Ble,
    SubGhz,
    Nrf24,
    Files,
    BadUsb,
    Voice,
    Vitals,
    Settings,
    Charge,
};

struct AppDescriptor {
    using LaunchCallback = void (*)();

    const char* name;
    AppIcon icon;
    LaunchCallback launch;
};
