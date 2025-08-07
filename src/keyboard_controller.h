#pragma once

#include <libremidi/libremidi.hpp>
#include <libremidi/ump.hpp>
#include <memory>
#include <vector>
#include <string>
#include "midi_ci_manager.h"

class KeyboardController {
public:
    KeyboardController();
    ~KeyboardController();
    
    bool resetMidiConnections();
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
    
    // MIDI-CI functionality
    void sendMidiCIDiscovery();
    std::vector<std::string> getMidiCIDevices();
    std::vector<MidiCIDeviceInfo> getMidiCIDeviceDetails();
    MidiCIDeviceInfo* getMidiCIDeviceByMuid(uint32_t muid);
    bool isMidiCIInitialized() const;
    uint32_t getMidiCIMuid() const;
    std::string getMidiCIDeviceName() const;
    void setMidiCIDevicesChangedCallback(std::function<void()> callback);
    
    // MIDI connection state
    bool hasValidMidiPair() const;
    void setMidiConnectionChangedCallback(std::function<void(bool)> callback);
    
private:
    std::unique_ptr<libremidi::midi_in> midiIn;
    std::unique_ptr<libremidi::midi_out> midiOut;
    std::unique_ptr<libremidi::observer> observer;
    std::unique_ptr<MidiCIManager> midiCIManager;
    
    std::string currentInputDeviceId;
    std::string currentOutputDeviceId;
    
    std::function<void(bool)> midiConnectionChangedCallback;
    bool initialized = false;
    
    void onMidiInput(libremidi::ump&& packet);
    
    // Helper functions for creating UMP packets
    libremidi::ump createUmpNoteOn(int channel, int note, int velocity);
    libremidi::ump createUmpNoteOff(int channel, int note);
    
    // MIDI-CI helper methods
    void initializeMidiCI();
    void processSysExForMidiCI(const std::vector<uint8_t>& sysex_data);
    bool sendSysExViaMidi(uint8_t group, const std::vector<uint8_t>& data);
    
    // Connection state helpers
    void updateUIConnectionState();
    bool previousConnectionState = false;
    
    // SysEx reconstruction state for multi-packet UMP SysEx7
    std::vector<uint8_t> sysex_buffer_;
    bool sysex_in_progress_ = false;
};