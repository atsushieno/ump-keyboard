
#include <QtWidgets/QApplication>
#include "keyboard_widget.h"
#include "keyboard_controller.h"
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    
    KeyboardWidget keyboard;
    KeyboardController controller;
    
    // Set up callbacks
    keyboard.setKeyPressedCallback([&controller](int note) {
        controller.noteOn(note, 80); // Default velocity
        std::cout << "Note ON: " << note << std::endl;
    });
    
    keyboard.setKeyReleasedCallback([&controller](int note) {
        controller.noteOff(note);
        std::cout << "Note OFF: " << note << std::endl;
    });
    
    keyboard.setDeviceRefreshCallback([&controller, &keyboard]() {
        auto inputDevices = controller.getInputDevices();
        auto outputDevices = controller.getOutputDevices();
        keyboard.updateMidiDevices(inputDevices, outputDevices);
    });
    
    // Connect device selection signals
    QObject::connect(&keyboard, &KeyboardWidget::midiInputDeviceChanged,
                    [&controller](const QString& deviceId) {
                        controller.selectInputDevice(deviceId.toStdString());
                    });
    
    QObject::connect(&keyboard, &KeyboardWidget::midiOutputDeviceChanged,
                    [&controller](const QString& deviceId) {
                        controller.selectOutputDevice(deviceId.toStdString());
                    });
    
    // Initialize device lists
    auto inputDevices = controller.getInputDevices();
    auto outputDevices = controller.getOutputDevices();
    keyboard.updateMidiDevices(inputDevices, outputDevices);
    
    keyboard.show();
    
    return app.exec();
}

