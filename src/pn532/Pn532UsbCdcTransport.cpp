// Pn532UsbCdcTransport - transport API, RX buffering and USB event pumping.
// Host install, enumeration and transfers live in Pn532UsbCdcHost.cpp.

#include "Pn532UsbCdcTransport.h"

#include "Pn532UsbCdcInternal.h"

using namespace Pn532UsbCdc;

Pn532UsbCdcTransport* Pn532UsbCdcTransport::active_ = nullptr;

Pn532UsbCdcTransport::Pn532UsbCdcTransport()
{
    rxBuffer_.reserve(rxLimit);
}

bool Pn532UsbCdcTransport::begin()
{
    if (hostInstalled_ || clientRegistered_ || device_) {
        end();
    }

    resetState();
    active_ = this;

    if (!installHost()) {
        return false;
    }

    const uint32_t startedAt = millis();

    while (!connected_ && millis() - startedAt < connectTimeoutMs) {
        handleClientEvents(20);

        if (pendingAddress_ != 0 && !device_) {
            openDevice(pendingAddress_);
        }

        delay(5);
    }

    if (!connected_) {
        setError("No USB serial device");
        return false;
    }

    return true;
}

void Pn532UsbCdcTransport::end()
{
    connected_ = false;

    if (inTransferBusy_ && device_ && inEndpoint_) {
        usb_host_endpoint_halt(device_, inEndpoint_);
        usb_host_endpoint_flush(device_, inEndpoint_);

        const uint32_t startedAt = millis();
        while (inTransferBusy_ && millis() - startedAt < 150) {
            handleClientEvents(10);
            delay(1);
        }
    }

    if (inTransfer_ && !inTransferBusy_) {
        usb_host_transfer_free(inTransfer_);
        inTransfer_ = nullptr;
    }

    if (outTransfer_) {
        usb_host_transfer_free(outTransfer_);
        outTransfer_ = nullptr;
    }

    if (controlTransfer_) {
        usb_host_transfer_free(controlTransfer_);
        controlTransfer_ = nullptr;
    }

    if (interfaceClaimed_ && device_ && client_) {
        usb_host_interface_release(client_, device_, interfaceNumber_);
        interfaceClaimed_ = false;
    }

    if (device_ && client_) {
        usb_host_device_close(client_, device_);
        device_ = nullptr;
    }

    if (clientRegistered_) {
        usb_host_client_unblock(client_);
        usb_host_client_deregister(client_);
        client_ = nullptr;
        clientRegistered_ = false;
    }

    if (hostTask_) {
        usb_host_lib_unblock();
        vTaskDelete(hostTask_);
        hostTask_ = nullptr;
    }

    if (hostInstalled_) {
        usb_host_device_free_all();
        usb_host_uninstall();
        hostInstalled_ = false;
    }

    if (active_ == this) {
        active_ = nullptr;
    }
}

size_t Pn532UsbCdcTransport::write(const uint8_t* data, size_t length)
{
    if (!connected_ || !data || length == 0) {
        return 0;
    }

    size_t written = 0;

    while (written < length) {
        const size_t chunk = std::min(length - written, transferSize);

        if (!submitOut(data + written, chunk)) {
            break;
        }

        written += chunk;
    }

    return written;
}

int Pn532UsbCdcTransport::read()
{
    available();

    if (rxBuffer_.empty()) {
        return -1;
    }

    const uint8_t value = rxBuffer_.front();
    rxBuffer_.erase(rxBuffer_.begin());
    return value;
}

int Pn532UsbCdcTransport::available()
{
    if (!connected_) {
        return 0;
    }

    handleClientEvents(1);

    if (!inTransferBusy_) {
        startReadTransfer();
    }

    return static_cast<int>(rxBuffer_.size());
}

void Pn532UsbCdcTransport::flush()
{
    handleClientEvents(5);
}

String Pn532UsbCdcTransport::lastError() const
{
    return lastError_;
}

void Pn532UsbCdcTransport::hostTaskThunk(void* arg)
{
    auto* self = static_cast<Pn532UsbCdcTransport*>(arg);

    while (self->hostInstalled_) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(pdMS_TO_TICKS(50), &flags);

        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }

    vTaskDelete(nullptr);
}

void Pn532UsbCdcTransport::clientEventThunk(const usb_host_client_event_msg_t* event, void* arg)
{
    auto* self = static_cast<Pn532UsbCdcTransport*>(arg);

    if (!self || !event) {
        return;
    }

    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        self->pendingAddress_ = event->new_dev.address;
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        self->connected_ = false;
        self->setError("USB serial disconnected");
    }
}

void Pn532UsbCdcTransport::transferThunk(usb_transfer_t* transfer)
{
    auto* self = static_cast<Pn532UsbCdcTransport*>(transfer ? transfer->context : nullptr);

    if (!self || !transfer) {
        return;
    }

    if (transfer == self->inTransfer_) {
        self->inTransferBusy_ = false;

        if (transfer->status == USB_TRANSFER_STATUS_COMPLETED && transfer->actual_num_bytes > 0) {
            self->appendRx(transfer->data_buffer, transfer->actual_num_bytes);
        }

        if (self->connected_) {
            self->startReadTransfer();
        }

        return;
    }

    self->transferDone_ = true;
}

bool Pn532UsbCdcTransport::waitTransfer(usb_transfer_t* transfer, uint32_t timeoutMs)
{
    const uint32_t startedAt = millis();

    while (!transferDone_ && millis() - startedAt < timeoutMs) {
        handleClientEvents(5);
        delay(1);
    }

    return transferDone_ && transfer && transfer->status == USB_TRANSFER_STATUS_COMPLETED;
}

void Pn532UsbCdcTransport::handleClientEvents(uint32_t timeoutMs)
{
    if (clientRegistered_ && client_) {
        usb_host_client_handle_events(client_, pdMS_TO_TICKS(timeoutMs));
    }
}

void Pn532UsbCdcTransport::appendRx(const uint8_t* data, size_t length)
{
    if (!data || length == 0) {
        return;
    }

    while (rxBuffer_.size() + length > rxLimit && !rxBuffer_.empty()) {
        rxBuffer_.erase(rxBuffer_.begin());
    }

    rxBuffer_.insert(rxBuffer_.end(), data, data + length);
}

void Pn532UsbCdcTransport::resetState()
{
    pendingAddress_ = 0;
    interfaceNumber_ = 0;
    inEndpoint_ = 0;
    outEndpoint_ = 0;
    inPacketSize_ = 64;
    outPacketSize_ = 64;
    interfaceClaimed_ = false;
    connected_ = false;
    inTransferBusy_ = false;
    transferDone_ = false;
    rxBuffer_.clear();
    lastError_ = "";
}

void Pn532UsbCdcTransport::setError(const String& message)
{
    lastError_ = message;
}
