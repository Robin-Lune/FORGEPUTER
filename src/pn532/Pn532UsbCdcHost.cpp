// Pn532UsbCdcTransport - USB host install, device enumeration and transfers.

#include "Pn532UsbCdcTransport.h"

#include <cstring>

#include "Pn532UsbCdcInternal.h"

using namespace Pn532UsbCdc;

namespace {
bool isBulkEndpoint(const usb_ep_desc_t* endpoint)
{
    return endpoint && USB_EP_DESC_GET_XFERTYPE(endpoint) == USB_TRANSFER_TYPE_BULK;
}

uint16_t ch34xBaudFactor(uint32_t baud)
{
    uint32_t factor = 1532620800UL / baud;
    uint8_t divisor = 3;

    while (factor > 0xFFF0 && divisor > 0) {
        factor >>= 3;
        divisor--;
    }

    factor = 0x10000 - factor;
    return static_cast<uint16_t>((factor & 0xFF00) | divisor);
}

uint16_t ch34xBaudRemainder(uint32_t baud)
{
    uint32_t factor = 1532620800UL / baud;
    uint8_t divisor = 3;

    while (factor > 0xFFF0 && divisor > 0) {
        factor >>= 3;
        divisor--;
    }

    factor = 0x10000 - factor;
    return static_cast<uint16_t>(factor & 0x00FF);
}
}

bool Pn532UsbCdcTransport::installHost()
{
    if (!hostInstalled_) {
        const usb_host_config_t hostConfig = {
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };

        esp_err_t err = usb_host_install(&hostConfig);

        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            setError("USB host install failed");
            return false;
        }

        hostInstalled_ = true;
    }

    if (!hostTask_) {
        xTaskCreate(hostTaskThunk, "pn532-usb-host", 4096, this, 2, &hostTask_);
    }

    const usb_host_client_config_t clientConfig = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = clientEventThunk,
            .callback_arg = this,
        },
    };

    if (usb_host_client_register(&clientConfig, &client_) != ESP_OK) {
        setError("USB host client failed");
        return false;
    }

    clientRegistered_ = true;

    if (usb_host_transfer_alloc(transferSize, 0, &inTransfer_) != ESP_OK ||
        usb_host_transfer_alloc(transferSize, 0, &outTransfer_) != ESP_OK ||
        usb_host_transfer_alloc(transferSize, 0, &controlTransfer_) != ESP_OK) {
        setError("USB transfer alloc failed");
        return false;
    }

    return true;
}

bool Pn532UsbCdcTransport::openDevice(uint8_t address)
{
    if (usb_host_device_open(client_, address, &device_) != ESP_OK) {
        return false;
    }

    const usb_device_desc_t* deviceDesc = nullptr;

    if (usb_host_get_device_descriptor(device_, &deviceDesc) != ESP_OK || !deviceDesc) {
        setError("USB descriptor failed");
        return false;
    }

    if (deviceDesc->idVendor != wchVid && deviceDesc->bDeviceClass != 0x02 && deviceDesc->bDeviceClass != 0xEF) {
        setError("USB device is not serial");
        return false;
    }

    return configureDevice();
}

bool Pn532UsbCdcTransport::configureDevice()
{
    const usb_config_desc_t* config = nullptr;

    if (usb_host_get_active_config_descriptor(device_, &config) != ESP_OK || !config) {
        setError("USB config failed");
        return false;
    }

    if (!findBulkInterface(config) || !claimInterface()) {
        return false;
    }

    configureCh34xBridge();

    const uint8_t lineCoding[] = {
        0x00, 0xC2, 0x01, 0x00,
        0x00,
        0x00,
        0x08,
    };
    submitControl(cdcRequestTypeOut, 0x20, 0x0000, interfaceNumber_, lineCoding, sizeof(lineCoding));

    connected_ = startReadTransfer();
    setError(connected_ ? "" : "USB read start failed");
    return connected_;
}

bool Pn532UsbCdcTransport::findBulkInterface(const usb_config_desc_t* config)
{
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(config);
    const uint8_t* end = ptr + config->wTotalLength;
    const usb_intf_desc_t* currentInterface = nullptr;
    uint8_t bulkIn = 0;
    uint8_t bulkOut = 0;
    uint16_t bulkInMps = 64;
    uint16_t bulkOutMps = 64;

    while (ptr + 2 <= end && ptr[0] >= 2) {
        if (ptr + ptr[0] > end) {
            break;
        }

        if (ptr[1] == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            if (currentInterface && bulkIn && bulkOut) {
                break;
            }

            currentInterface = reinterpret_cast<const usb_intf_desc_t*>(ptr);
            bulkIn = 0;
            bulkOut = 0;
        } else if (ptr[1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT && currentInterface) {
            const auto* endpoint = reinterpret_cast<const usb_ep_desc_t*>(ptr);

            if (isBulkEndpoint(endpoint)) {
                if (USB_EP_DESC_GET_EP_DIR(endpoint)) {
                    bulkIn = endpoint->bEndpointAddress;
                    bulkInMps = USB_EP_DESC_GET_MPS(endpoint);
                } else {
                    bulkOut = endpoint->bEndpointAddress;
                    bulkOutMps = USB_EP_DESC_GET_MPS(endpoint);
                }
            }
        }

        ptr += ptr[0];
    }

    if (!currentInterface || !bulkIn || !bulkOut) {
        setError("No USB serial endpoints");
        return false;
    }

    interfaceNumber_ = currentInterface->bInterfaceNumber;
    inEndpoint_ = bulkIn;
    outEndpoint_ = bulkOut;
    inPacketSize_ = bulkInMps ? bulkInMps : 64;
    outPacketSize_ = bulkOutMps ? bulkOutMps : 64;
    return true;
}

bool Pn532UsbCdcTransport::claimInterface()
{
    if (usb_host_interface_claim(client_, device_, interfaceNumber_, 0) != ESP_OK) {
        setError("USB interface busy");
        return false;
    }

    interfaceClaimed_ = true;
    return true;
}

bool Pn532UsbCdcTransport::configureCh34xBridge()
{
    bool ok = true;

    ok = submitControl(requestTypeOut, ch34xRequestSerialInit, 0x0000, 0x0000, nullptr, 0) && ok;
    ok = submitControl(requestTypeOut, ch34xRequestWriteReg, 0x1312, ch34xBaudFactor(serialBaud), nullptr, 0) && ok;
    ok = submitControl(requestTypeOut, ch34xRequestWriteReg, 0x0F2C, ch34xBaudRemainder(serialBaud), nullptr, 0) && ok;
    ok = submitControl(requestTypeOut, ch34xRequestWriteReg, 0x2518, 0x00C3, nullptr, 0) && ok;
    ok = submitControl(requestTypeOut, ch34xRequestModemCtrl, 0x00FF, 0x0000, nullptr, 0) && ok;

    return ok;
}

bool Pn532UsbCdcTransport::startReadTransfer()
{
    if (!device_ || !inTransfer_ || inTransferBusy_) {
        return connected_;
    }

    inTransfer_->device_handle = device_;
    inTransfer_->bEndpointAddress = inEndpoint_;
    inTransfer_->num_bytes = std::min<size_t>(transferSize, inPacketSize_);
    inTransfer_->callback = transferThunk;
    inTransfer_->context = this;
    inTransfer_->timeout_ms = 0;
    inTransfer_->flags = 0;

    if (usb_host_transfer_submit(inTransfer_) != ESP_OK) {
        return false;
    }

    inTransferBusy_ = true;
    return true;
}

bool Pn532UsbCdcTransport::submitControl(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, const uint8_t* data, size_t length)
{
    if (!controlTransfer_ || length + sizeof(usb_setup_packet_t) > controlTransfer_->data_buffer_size) {
        return false;
    }

    auto* setup = reinterpret_cast<usb_setup_packet_t*>(controlTransfer_->data_buffer);
    setup->bmRequestType = requestType;
    setup->bRequest = request;
    setup->wValue = value;
    setup->wIndex = index;
    setup->wLength = length;

    if (data && length > 0) {
        memcpy(controlTransfer_->data_buffer + sizeof(usb_setup_packet_t), data, length);
    }

    controlTransfer_->device_handle = device_;
    controlTransfer_->bEndpointAddress = 0;
    controlTransfer_->num_bytes = sizeof(usb_setup_packet_t) + length;
    controlTransfer_->callback = transferThunk;
    controlTransfer_->context = this;
    controlTransfer_->timeout_ms = transferTimeoutMs;
    controlTransfer_->flags = 0;
    transferDone_ = false;

    if (usb_host_transfer_submit_control(client_, controlTransfer_) != ESP_OK) {
        return false;
    }

    return waitTransfer(controlTransfer_, transferTimeoutMs);
}

bool Pn532UsbCdcTransport::submitOut(const uint8_t* data, size_t length)
{
    if (!outTransfer_ || length > outTransfer_->data_buffer_size) {
        return false;
    }

    memcpy(outTransfer_->data_buffer, data, length);
    outTransfer_->device_handle = device_;
    outTransfer_->bEndpointAddress = outEndpoint_;
    outTransfer_->num_bytes = length;
    outTransfer_->callback = transferThunk;
    outTransfer_->context = this;
    outTransfer_->timeout_ms = transferTimeoutMs;
    outTransfer_->flags = 0;
    transferDone_ = false;

    if (usb_host_transfer_submit(outTransfer_) != ESP_OK) {
        setError("USB write submit failed");
        return false;
    }

    if (!waitTransfer(outTransfer_, transferTimeoutMs) || outTransfer_->status != USB_TRANSFER_STATUS_COMPLETED) {
        setError("USB write failed");
        return false;
    }

    return true;
}
