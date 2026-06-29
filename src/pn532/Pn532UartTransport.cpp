#include "Pn532UartTransport.h"

#ifndef FORGE_PN532_UART_BAUD
#define FORGE_PN532_UART_BAUD 115200
#endif

#ifndef FORGE_PN532_RX_PIN
#define FORGE_PN532_RX_PIN 1
#endif

#ifndef FORGE_PN532_TX_PIN
#define FORGE_PN532_TX_PIN 2
#endif

Pn532UartTransport::Pn532UartTransport(HardwareSerial& serial)
    : serial_(serial)
{
}

bool Pn532UartTransport::begin()
{
    serial_.begin(FORGE_PN532_UART_BAUD, SERIAL_8N1, FORGE_PN532_RX_PIN, FORGE_PN532_TX_PIN);
    delay(40);
    return true;
}

void Pn532UartTransport::end()
{
    serial_.end();
}

size_t Pn532UartTransport::write(const uint8_t* data, size_t length)
{
    return serial_.write(data, length);
}

int Pn532UartTransport::read()
{
    return serial_.read();
}

int Pn532UartTransport::available()
{
    return serial_.available();
}

void Pn532UartTransport::flush()
{
    serial_.flush();
}
