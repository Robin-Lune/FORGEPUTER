// Pn532KillerApp - keyboard routing per view and list cursor moves.

#include "Pn532KillerApp.h"

void Pn532KillerApp::onKey(const KeyInput& input)
{
    if (view_ == View::BleEmulate) {
        if (input.del || input.backspace) {
            stopBleNdefEmulation();
        }

        return;
    }

    if (view_ == View::EmulateNdefEdit) {
        if (input.del) {
            goBack();
            return;
        }

        if (input.backspace) {
            if (emulateBuffer_.length() > 0) {
                emulateBuffer_.remove(emulateBuffer_.length() - 1);
            }

            draw();
            return;
        }

        if (input.tab || input.left || input.right) {
            emulateUrlMode_ = !emulateUrlMode_;
            draw();
            return;
        }

        if (input.enter) {
            startBleEditedNdefEmulation();
            return;
        }

        if (input.characters.length() > 0) {
            emulateBuffer_ += input.characters;
            draw();
        }

        return;
    }

    if (view_ == View::SaveName) {
        if (input.del) {
            view_ = View::CardActions;
            draw();
            return;
        }

        if (input.backspace) {
            if (saveName_.length() > 0) {
                saveName_.remove(saveName_.length() - 1);
            }

            draw();
            return;
        }

        if (input.enter) {
            saveCurrentDump();
            return;
        }

        if (input.characters.length() > 0 && saveName_.length() < 28) {
            saveName_ += input.characters;
            draw();
        }

        return;
    }

    if (view_ == View::WriteTag) {
        if (input.del) {
            goBack();
            return;
        }

        if (input.backspace) {
            if (writeBuffer_.length() > 0) {
                writeBuffer_.remove(writeBuffer_.length() - 1);
            }

            draw();
            return;
        }

        if (input.tab) {
            writeUrlMode_ = !writeUrlMode_;
            draw();
            return;
        }

        if (input.enter) {
            writeCurrentNdef();
            return;
        }

        if (input.left || input.right) {
            writeUrlMode_ = !writeUrlMode_;
            draw();
            return;
        }

        if (input.characters.length() > 0) {
            writeBuffer_ += input.characters;
            draw();
        }

        return;
    }

    if (view_ == View::ReadProgress && (slotUploadResultVisible_ || ntagDiagnosticVisible_)) {
        if (ntagDiagnosticVisible_ && (input.up || input.down)) {
            const int maxOffset = max(0, static_cast<int>(readLog_.size()) - 3);

            if (input.up && readLogOffset_ > 0) {
                readLogOffset_--;
                drawReadProgress();
            }

            if (input.down && readLogOffset_ < maxOffset) {
                readLogOffset_++;
                drawReadProgress();
            }

            return;
        }

        if (input.enter || input.del || input.backspace) {
            if (slotUploadResultVisible_) {
                slotUploadResultVisible_ = false;
                view_ = View::Slots;
            } else {
                ntagDiagnosticVisible_ = false;
                view_ = diagnosticReturnView_;
            }
            selectedIndex_ = 0;
            listOffset_ = 0;
            draw();
        }

        return;
    }

    if (input.backspace || input.del) {
        goBack();
        return;
    }

    if (view_ == View::CardInfo || view_ == View::MissingUnits) {
        const int lineCount = view_ == View::CardInfo ? cardInfoLineCount() : missingUnitLineCount();
        const int maxOffset = max(0, lineCount - 4);

        if (input.up) {
            if (resultOffset_ > 0) {
                resultOffset_--;
                draw();
            }

            return;
        }

        if (input.down) {
            if (resultOffset_ < maxOffset) {
                resultOffset_++;
                draw();
            }

            return;
        }
    }

    if (input.up) {
        moveSelection(-1);
        return;
    }

    if (input.down) {
        moveSelection(1);
        return;
    }

    if (input.left && view_ == View::Slots) {
        selectedSlot_ = selectedSlot_ == 0 ? 7 : selectedSlot_ - 1;
        draw();
        return;
    }

    if (input.right && view_ == View::Slots) {
        selectedSlot_ = (selectedSlot_ + 1) % 8;
        draw();
        return;
    }

    if (input.tab && view_ == View::Slots) {
        if (pendingSlotUpload_) {
            status_ = "Type follows dump";
            draw();
            return;
        }

        cycleSlotType();
        return;
    }

    if (input.enter) {
        openSelectedItem();
    }
}

void Pn532KillerApp::moveSelection(int delta)
{
    int count = mainMenuCount();

    if (view_ == View::Transport) {
        count = transportCount_;
    } else if (view_ == View::CardActions) {
        count = cardActionCount();
    } else if (view_ == View::DumpList) {
        count = dumpEntries_.size();
    } else if (view_ == View::DumpDetail) {
        count = dumpActionCount();
    } else if (view_ == View::EmulateNdefEdit) {
        return;
    } else if (view_ == View::Slots) {
        count = 4;
    } else if (view_ == View::Lab) {
        count = 3;
    } else if (view_ != View::Menu) {
        return;
    }

    if (count == 0) {
        return;
    }

    selectedIndex_ += delta;

    if (selectedIndex_ < 0) {
        selectedIndex_ = count - 1;
    }

    if (selectedIndex_ >= count) {
        selectedIndex_ = 0;
    }

    updateListOffset(count);
    draw();
}
