// Shim Arduino minimal pour la simulation desktop SDL.
// Ne couvre que ce que le sous-ensemble UI de Forgeputer utilise réellement.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include <M5GFX.h>

inline unsigned long millis()
{
    return lgfx::v1::millis();
}

inline unsigned long micros()
{
    return lgfx::v1::micros();
}

inline void delay(unsigned long ms)
{
    lgfx::v1::delay(ms);
}

inline void delayMicroseconds(unsigned int us)
{
    lgfx::v1::delayMicroseconds(us);
}

// Sous-ensemble de l'API Arduino String utilisé par Forgeputer.
class String {
public:
    String() = default;
    String(const char* value) : value_(value != nullptr ? value : "") {}
    String(const std::string& value) : value_(value) {}

    String& operator+=(char c)
    {
        value_ += c;
        return *this;
    }

    String& operator+=(const char* s)
    {
        if (s != nullptr) {
            value_ += s;
        }
        return *this;
    }

    String& operator+=(const String& other)
    {
        value_ += other.value_;
        return *this;
    }

    int indexOf(char c) const
    {
        const auto pos = value_.find(c);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    int indexOf(const char* s) const
    {
        if (s == nullptr) {
            return -1;
        }
        const auto pos = value_.find(s);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    size_t length() const { return value_.length(); }
    bool isEmpty() const { return value_.empty(); }
    const char* c_str() const { return value_.c_str(); }
    operator const char*() const { return value_.c_str(); }

    bool operator==(const String& other) const { return value_ == other.value_; }
    bool operator!=(const String& other) const { return value_ != other.value_; }

private:
    std::string value_;
};

inline String operator+(const String& a, const String& b)
{
    String out = a;
    out += b;
    return out;
}
