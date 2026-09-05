// Pn532KillerApp - view transitions: open, back, main menu routing, list offsets.

#include "Pn532KillerApp.h"

void Pn532KillerApp::openSelectedItem()
{
    if (view_ == View::Transport) {
        selectTransport();
        return;
    }

    if (view_ == View::Menu) {
        openMainMenuItem();
        return;
    }

    if (view_ == View::CardInfo) {
        if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() == 0) {
            goBack();
            return;
        }

        view_ = View::CardActions;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::CardActions) {
        performCardAction();
        return;
    }

    if (view_ == View::RetryResult) {
        view_ = View::CardInfo;
        selectedIndex_ = 0;
        listOffset_ = 0;
        resultOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::ScanInfo) {
        scanInfo();
        return;
    }

    if (view_ == View::DumpList) {
        if (selectingDumpForSlot_) {
            loadSelectedDumpForSlot();
        } else {
            loadSelectedDump();
        }
        return;
    }

    if (view_ == View::DumpDetail) {
        performDumpAction();
        return;
    }

    if (view_ == View::MissingUnits) {
        view_ = View::DumpDetail;
        selectedIndex_ = 0;
        listOffset_ = 0;
        resultOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::Slots) {
        if (pendingSlotUpload_) {
            if (selectedIndex_ == 0) uploadCurrentDumpToSlot();
            if (selectedIndex_ == 1) openSlotDumpPicker();
            if (selectedIndex_ == 2) {
                pendingSlotUpload_ = false;
                currentDump_ = NfcDump();
                status_ = "Upload canceled";
                draw();
            }
            if (selectedIndex_ == 3) {
                pendingSlotUpload_ = false;
                currentDump_ = NfcDump();
                goBack();
            }
            return;
        }

        if (selectedIndex_ == 0) startSlot();
        if (selectedIndex_ == 1) stopSlot();
        if (selectedIndex_ == 2) openSlotDumpPicker();
        if (selectedIndex_ == 3) goBack();
        return;
    }

    if (view_ == View::Lab) {
        if (selectedIndex_ == 0) runRawPn532Diagnostic();
        if (selectedIndex_ == 1 || selectedIndex_ == 2) {
            status_ = "Lab: next";
            draw();
        }
        return;
    }
}

void Pn532KillerApp::goBack()
{
    if (view_ == View::Transport) {
        return;
    }

    if (view_ == View::Menu) {
        releaseBleConnection();
        transport_.end();
        usbTransport_.end();
        view_ = View::Transport;
        transportMode_ = TransportMode::None;
        pn532Ready_ = false;
        status_ = "Choose link";
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::DumpDetail) {
        selectingDumpForSlot_ = false;
        view_ = View::DumpList;
    } else if (view_ == View::DumpList && selectingDumpForSlot_) {
        selectingDumpForSlot_ = false;
        view_ = View::Slots;
    } else if (view_ == View::Slots && pendingSlotUpload_) {
        pendingSlotUpload_ = false;
        currentDump_ = NfcDump();
        view_ = View::Menu;
    } else if (view_ == View::MissingUnits) {
        view_ = View::DumpDetail;
    } else if (view_ == View::EmulateNdefEdit) {
        view_ = emulateFromDump_ ? View::DumpDetail : View::Menu;
    } else if (view_ == View::SaveName) {
        view_ = View::CardActions;
    } else if (view_ == View::RetryResult) {
        view_ = View::CardInfo;
    } else if (view_ == View::CardActions) {
        view_ = View::CardInfo;
    } else {
        view_ = View::Menu;
    }

    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::openMainMenuItem()
{
    const String item = mainMenuLabel(selectedIndex_);

    if (item == "Scan Info") {
        scanInfo();
        return;
    }

    if (item == "Read Auto") {
        readAuto();
        return;
    }

    if (item == "Saved Dumps") {
        refreshDumpList();
        view_ = View::DumpList;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (item == "Killer Slots") {
        view_ = View::Slots;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (item == "Write Tag") {
        view_ = View::WriteTag;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (item == "Emulate NDEF") {
        openBleNdefEditor(false);
        return;
    }

    view_ = View::Lab;
    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::updateListOffset(int count)
{
    const int visibleRows = visibleRowsForCurrentView();

    if (selectedIndex_ < listOffset_) {
        listOffset_ = selectedIndex_;
    }

    if (selectedIndex_ >= listOffset_ + visibleRows) {
        listOffset_ = selectedIndex_ - visibleRows + 1;
    }

    if (listOffset_ > count - visibleRows) {
        listOffset_ = max(0, count - visibleRows);
    }
}

int Pn532KillerApp::visibleRowsForCurrentView() const
{
    if (view_ == View::CardActions || view_ == View::DumpDetail || view_ == View::Slots) {
        return 2;
    }

    return visibleItemCount_;
}
