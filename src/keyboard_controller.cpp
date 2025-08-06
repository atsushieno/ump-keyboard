#include "keyboard_controller.h"
#include <iostream>
#include <algorithm>
#include <libremidi/ump.hpp>

KeyboardController::KeyboardController() {
    initialize();
}

KeyboardController::~KeyboardController() {
    if (initialized) {
        allNotesOff();
        if (midiIn && midiIn->is_port_open()) {
            midiIn->close_port();
        }
        if (midiOut && midiOut->is_port_open()) {
            midiOut->close_port();
        }
    }
}

bool KeyboardController::initialize() {
    try {
        // Create observer with UMP/MIDI 2.0 configuration for device detection
        libremidi::observer_configuration obsConf;
        obsConf.track_hardware = true;   // Track hardware MIDI devices
        obsConf.track_virtual = true;    // Track virtual/software MIDI devices  
        obsConf.track_any = true;        // Track any other types of devices
        obsConf.notify_in_constructor = true;  // Get existing ports immediately
        
        // Add callbacks for device hotplug detection
        obsConf.input_added = [this](const libremidi::input_port& port) {
            std::cout << "MIDI Input device connected: " << port.port_name << std::endl;
        };
        obsConf.input_removed = [this](const libremidi::input_port& port) {
            std::cout << "MIDI Input device disconnected: " << port.port_name << std::endl;
        };
        obsConf.output_added = [this](const libremidi::output_port& port) {
            std::cout << "MIDI Output device connected: " << port.port_name << std::endl;
        };
        obsConf.output_removed = [this](const libremidi::output_port& port) {
            std::cout << "MIDI Output device disconnected: " << port.port_name << std::endl;
        };
        
        // Use MIDI 2.0/UMP observer configuration
        observer = std::make_unique<libremidi::observer>(obsConf, libremidi::midi2::observer_default_configuration());
        
        // Create MIDI input with UMP callback configuration
        libremidi::ump_input_configuration inConf;
        inConf.on_message = [this](libremidi::ump&& packet) {
            onMidiInput(std::move(packet));
        };
        midiIn = std::make_unique<libremidi::midi_in>(inConf, libremidi::midi2::in_default_configuration());
        
        // Create MIDI output with UMP configuration
        libremidi::output_configuration outConf;
        midiOut = std::make_unique<libremidi::midi_out>(outConf, libremidi::midi2::out_default_configuration());
        
        // Start with virtual output port
        selectOutputDevice("virtual");
        
        initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "MIDI initialization failed: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::pair<std::string, std::string>> KeyboardController::getInputDevices() {
    std::vector<std::pair<std::string, std::string>> devices;
    
    try {
        if (!observer) {
            std::cerr << "Observer not initialized" << std::endl;
            return devices;
        }
        
        auto ports = observer->get_input_ports();
        for (size_t i = 0; i < ports.size(); i++) {
            std::string id = std::to_string(i);
            std::string name = ports[i].port_name;
            // Device now supports UMP/MIDI 2.0 by default with the UMP backend
            devices.emplace_back(id, name);
        }
        
        std::cout << "Found " << devices.size() << " input devices" << std::endl;
        for (const auto& device : devices) {
            std::cout << "  ID: " << device.first << " - " << device.second << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting input devices: " << e.what() << std::endl;
    }
    
    return devices;
}

std::vector<std::pair<std::string, std::string>> KeyboardController::getOutputDevices() {
    std::vector<std::pair<std::string, std::string>> devices;
    
    try {
        if (!observer) {
            std::cerr << "Observer not initialized" << std::endl;
            return devices;
        }
        
        auto ports = observer->get_output_ports();
        for (size_t i = 0; i < ports.size(); i++) {
            std::string id = std::to_string(i);
            std::string name = ports[i].port_name;
            // Device now supports UMP/MIDI 2.0 by default with the UMP backend
            devices.emplace_back(id, name);
        }
        
        std::cout << "Found " << devices.size() << " output devices" << std::endl;
        for (const auto& device : devices) {
            std::cout << "  ID: " << device.first << " - " << device.second << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting output devices: " << e.what() << std::endl;
    }
    
    return devices;
}

bool KeyboardController::selectInputDevice(const std::string& deviceId) {
    try {
        if (midiIn && midiIn->is_port_open()) {
            midiIn->close_port();
        }
        
        if (deviceId.empty()) {
            std::cout << "No input device selected" << std::endl;
            currentInputDeviceId = "";
            return true;
        }
        
        auto ports = observer->get_input_ports();
        size_t portIndex = std::stoul(deviceId);
        
        if (portIndex < ports.size()) {
            midiIn->open_port(ports[portIndex]);
            currentInputDeviceId = deviceId;
            std::cout << "Opened input device: " << ports[portIndex].port_name << std::endl;
            return true;
        } else {
            std::cerr << "Invalid input device ID: " << deviceId << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error selecting input device: " << e.what() << std::endl;
        return false;
    }
}

bool KeyboardController::selectOutputDevice(const std::string& deviceId) {
    try {
        if (midiOut && midiOut->is_port_open()) {
            midiOut->close_port();
        }
        
        if (deviceId == "virtual") {
            midiOut->open_virtual_port("UMP Keyboard");
            currentOutputDeviceId = deviceId;
            std::cout << "Created virtual MIDI output port: UMP Keyboard" << std::endl;
            return true;
        }
        
        auto ports = observer->get_output_ports();
        size_t portIndex = std::stoul(deviceId);
        
        if (portIndex < ports.size()) {
            midiOut->open_port(ports[portIndex]);
            currentOutputDeviceId = deviceId;
            std::cout << "Opened output device: " << ports[portIndex].port_name << std::endl;
            return true;
        } else {
            std::cerr << "Invalid output device ID: " << deviceId << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error selecting output device: " << e.what() << std::endl;
        return false;
    }
}

void KeyboardController::refreshDevices() {
    std::cout << "Refreshing MIDI devices..." << std::endl;
    // The observer automatically refreshes device lists
    // We just need to call the getter functions to update
    getInputDevices();
    getOutputDevices();
}

void KeyboardController::noteOn(int note, int velocity) {
    if (!midiOut || !initialized) return;
    
    try {
        // Send MIDI 2.0 UMP note on message
        libremidi::ump noteOnPacket = createUmpNoteOn(0, note, velocity);
        midiOut->send_ump(noteOnPacket);
    } catch (const std::exception& e) {
        std::cerr << "Error sending note on: " << e.what() << std::endl;
    }
}

void KeyboardController::noteOff(int note) {
    if (!midiOut || !initialized) return;
    
    try {
        // Send MIDI 2.0 UMP note off message
        libremidi::ump noteOffPacket = createUmpNoteOff(0, note);
        midiOut->send_ump(noteOffPacket);
    } catch (const std::exception& e) {
        std::cerr << "Error sending note off: " << e.what() << std::endl;
    }
}

void KeyboardController::allNotesOff() {
    if (!midiOut || !initialized) return;
    
    try {
        // Send all notes off message
        for (int note = 0; note < 128; note++) {
            noteOff(note);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error sending all notes off: " << e.what() << std::endl;
    }
}

void KeyboardController::onMidiInput(libremidi::ump&& packet) {
    // Handle incoming UMP packets
    // This could be used for MIDI input from external devices
    std::cout << "UMP Input: ";
    for (int i = 0; i < 4; i++) {
        std::cout << std::hex << packet.data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

libremidi::ump KeyboardController::createUmpNoteOn(int channel, int note, int velocity) {
    // Create MIDI 2.0 Note On UMP packet
    // Format: [Message Type (4) | Channel (4) | Status (8) | Note (8) | Reserved (8)] [Velocity (16) | Attribute Type (8) | Velocity MSB (8)] [0] [0]
    uint32_t word0 = (0x4 << 28) | (channel << 24) | (0x90 << 16) | (note << 8) | 0x00;
    uint32_t word1 = (velocity << 24) | (velocity << 16); // MIDI 2.0 uses 16-bit velocity
    return libremidi::ump(word0, word1, 0, 0);
}

libremidi::ump KeyboardController::createUmpNoteOff(int channel, int note) {
    // Create MIDI 2.0 Note Off UMP packet
    // Format: [Message Type (4) | Channel (4) | Status (8) | Note (8) | Reserved (8)] [Velocity (16) | Attribute Type (8) | Velocity MSB (8)] [0] [0]
    uint32_t word0 = (0x4 << 28) | (channel << 24) | (0x80 << 16) | (note << 8) | 0x00;
    uint32_t word1 = 0; // Zero velocity for note off
    return libremidi::ump(word0, word1, 0, 0);
}