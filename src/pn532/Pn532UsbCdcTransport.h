#pragma once

#include "Pn532Transport.h"

#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

class Pn532UsbCdcTransport : public Pn532Transport {
public:
    Pn532UsbCdcTransport();

    bool begin() override;
    void end() override;
    size_t write(const uint8_t* data, size_t length) override;
    int read() override;
    int available() override;
    void flush() override;
    String lastError() const;

private:
    static Pn532UsbCdcTransport* active_;
    static void hostTaskThunk(void* arg);
    static void clientEventThunk(const usb_host_client_event_msg_t* event, void* arg);
    static void transferThunk(usb_transfer_t* transfer);

    bool installHost();
    bool openDevice(uint8_t address);
    bool configureDevice();
    bool findBulkInterface(const usb_config_desc_t* config);
    bool claimInterface();
    bool configureCh34xBridge();
    bool startReadTransfer();
    bool submitControl(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, const uint8_t* data, size_t length);
    bool submitOut(const uint8_t* data, size_t length);
    bool waitTransfer(usb_transfer_t* transfer, uint32_t timeoutMs);
    void handleClientEvents(uint32_t timeoutMs);
    void appendRx(const uint8_t* data, size_t length);
    void resetState();
    void setError(const String& message);

    TaskHandle_t hostTask_ = nullptr;
    usb_host_client_handle_t client_ = nullptr;
    usb_device_handle_t device_ = nullptr;
    usb_transfer_t* inTransfer_ = nullptr;
    usb_transfer_t* outTransfer_ = nullptr;
    usb_transfer_t* controlTransfer_ = nullptr;
    uint8_t pendingAddress_ = 0;
    uint8_t interfaceNumber_ = 0;
    uint8_t inEndpoint_ = 0;
    uint8_t outEndpoint_ = 0;
    uint16_t inPacketSize_ = 64;
    uint16_t outPacketSize_ = 64;
    bool hostInstalled_ = false;
    bool clientRegistered_ = false;
    bool interfaceClaimed_ = false;
    bool connected_ = false;
    bool inTransferBusy_ = false;
    bool transferDone_ = false;
    std::vector<uint8_t> rxBuffer_;
    String lastError_;
};
