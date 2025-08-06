#pragma once

#include <libremidi/libremidi.hpp>
#include <libremidi/ump.hpp>
#include <memory>
#include <vector>
#include <string>

class KeyboardController {
public:
    KeyboardController();
    ~KeyboardController();
    
    bool initialize();
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();
    
    // Device enumeration
    std::vector<std::pair<std::string, std::string>> getInputDevices();
    std::vector<std::pair<std::string, std::string>> getOutputDevices();
    
    // Device selection
    bool selectInputDevice(const std::string& deviceId);
    bool selectOutputDevice(const std::string& deviceId);
    
    void refreshDevices();
    
private:
    std::unique_ptr<libremidi::midi_in> midiIn;
    std::unique_ptr<libremidi::midi_out> midiOut;
    std::unique_ptr<libremidi::observer> observer;
    
    std::string currentInputDeviceId;
    std::string currentOutputDeviceId;
    
    bool initialized = false;
    
    void onMidiInput(libremidi::ump&& packet);
    
    // Helper functions for creating UMP packets
    libremidi::ump createUmpNoteOn(int channel, int note, int velocity);
    libremidi::ump createUmpNoteOff(int channel, int note);
};