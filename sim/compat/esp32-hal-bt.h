// Shim Bluetooth ESP32 pour la simulation desktop. Sans effet.
#pragma once

inline bool btStart()
{
    return true;
}

inline bool btStop()
{
    return true;
}
