#include "keyboard_controller.h"
#include <iostream>
#include <algorithm>
#include <libremidi/ump.hpp>
#include <cmidi2.h>

KeyboardController::KeyboardController() {
    resetMidiConnections();
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
        if (midiCIManager) {
            midiCIManager->shutdown();
        }
    }
}

bool KeyboardController::resetMidiConnections() {
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
        libremidi::ump_input_configuration inConf {
            .on_message = [this](libremidi::ump&& packet) {
                onMidiInput(std::move(packet));
            },
            .ignore_sysex = false
        };

        midiIn = std::make_unique<libremidi::midi_in>(inConf, libremidi::midi2::in_default_configuration());
        
        // Create MIDI output with UMP configuration
        libremidi::output_configuration outConf;
        midiOut = std::make_unique<libremidi::midi_out>(outConf, libremidi::midi2::out_default_configuration());
        
        
        // Initialize MIDI-CI
        initializeMidiCI();
        
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
            updateUIConnectionState();
        }
        
        if (deviceId.empty()) {
            currentInputDeviceId = "";
            updateUIConnectionState();
            return true;
        }
        
        auto ports = observer->get_input_ports();
        size_t portIndex = std::stoul(deviceId);
        
        if (portIndex < ports.size()) {
            midiIn->open_port(ports[portIndex]);
            currentInputDeviceId = deviceId;
            updateUIConnectionState();
            
            // Reinitialize MIDI-CI when we have a valid MIDI pair
            if (hasValidMidiPair()) {
                initializeMidiCI();
            }
            
            return true;
        } else {
            return false;
        }
    } catch (const std::exception& e) {
        return false;
    }
}

bool KeyboardController::selectOutputDevice(const std::string& deviceId) {
    try {
        if (midiOut && midiOut->is_port_open()) {
            midiOut->close_port();
            updateUIConnectionState();
        }
        
        if (deviceId.empty()) {
            currentOutputDeviceId = "";
            updateUIConnectionState();
            return true;
        }
        
        auto ports = observer->get_output_ports();
        size_t portIndex = std::stoul(deviceId);
        
        if (portIndex < ports.size()) {
            midiOut->open_port(ports[portIndex]);
            currentOutputDeviceId = deviceId;
            updateUIConnectionState();
            
            // Reinitialize MIDI-CI when we have a valid MIDI pair
            if (hasValidMidiPair()) {
                initializeMidiCI();
            }
            
            return true;
        } else {
            return false;
        }
    } catch (const std::exception& e) {
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
    std::cout << "[UMP INPUT] Raw packet: ";
    for (int i = 0; i < 4; i++) {
        std::cout << std::hex << "0x" << packet.data[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Check if this is a System Exclusive message (UMP Type 3 - SysEx7)
    uint8_t message_type = (packet.data[0] >> 28) & 0xF;
    if (message_type == 0x3) { // SysEx7 UMP
        std::cout << "[SYSEX DETECTED] UMP SysEx7 message type 3" << std::endl;
        
        uint8_t group = (packet.data[0] >> 24) & 0xF;
        uint8_t status = (packet.data[0] >> 20) & 0xF;
        uint8_t number_of_bytes = (packet.data[0] >> 16) & 0xF;
        
        std::cout << "[SYSEX INFO] Group: " << (int)group << ", Status: " << (int)status << ", Bytes: " << (int)number_of_bytes << std::endl;
        
        // Handle multi-packet SysEx reconstruction manually based on UMP SysEx7 status
        switch (status) {
            case 0x0: { // Complete SysEx in one packet
                std::cout << "[SYSEX] Complete in one packet" << std::endl;
                sysex_buffer_.clear();
                sysex_buffer_.push_back(0xF0); // Add SysEx start
                
                // Extract data bytes using cmidi2 helper functions
                uint64_t ump_data = (static_cast<uint64_t>(packet.data[0]) << 32) | static_cast<uint64_t>(packet.data[1]);
                for (int i = 0; i < number_of_bytes && i < 6; i++) {
                    uint8_t data_byte = cmidi2_ump_get_byte_from_uint64(ump_data, 2 + i);
                    sysex_buffer_.push_back(data_byte);
                }
                sysex_buffer_.push_back(0xF7); // Add SysEx end
                
                processSysExForMidiCI(sysex_buffer_);
                sysex_in_progress_ = false;
                break;
            }
            case 0x1: { // SysEx start
                std::cout << "[SYSEX] Start packet" << std::endl;
                sysex_buffer_.clear();
                sysex_buffer_.push_back(0xF0); // Add SysEx start
                sysex_in_progress_ = true;
                
                // Extract data bytes
                uint64_t ump_data = (static_cast<uint64_t>(packet.data[0]) << 32) | static_cast<uint64_t>(packet.data[1]);
                for (int i = 0; i < number_of_bytes && i < 6; i++) {
                    uint8_t data_byte = cmidi2_ump_get_byte_from_uint64(ump_data, 2 + i);
                    sysex_buffer_.push_back(data_byte);
                }
                break;
            }
            case 0x2: { // SysEx continue
                std::cout << "[SYSEX] Continue packet" << std::endl;
                if (!sysex_in_progress_) {
                    std::cerr << "[SYSEX ERROR] Continue packet without start" << std::endl;
                    break;
                }
                
                // Extract data bytes
                uint64_t ump_data = (static_cast<uint64_t>(packet.data[0]) << 32) | static_cast<uint64_t>(packet.data[1]);
                for (int i = 0; i < number_of_bytes && i < 6; i++) {
                    uint8_t data_byte = cmidi2_ump_get_byte_from_uint64(ump_data, 2 + i);
                    sysex_buffer_.push_back(data_byte);
                }
                break;
            }
            case 0x3: { // SysEx end
                std::cout << "[SYSEX] End packet" << std::endl;
                if (!sysex_in_progress_) {
                    std::cerr << "[SYSEX ERROR] End packet without start" << std::endl;
                    break;
                }
                
                // Extract final data bytes
                uint64_t ump_data = (static_cast<uint64_t>(packet.data[0]) << 32) | static_cast<uint64_t>(packet.data[1]);
                for (int i = 0; i < number_of_bytes && i < 6; i++) {
                    uint8_t data_byte = cmidi2_ump_get_byte_from_uint64(ump_data, 2 + i);
                    sysex_buffer_.push_back(data_byte);
                }
                sysex_buffer_.push_back(0xF7); // Add SysEx end
                
                processSysExForMidiCI(sysex_buffer_);
                sysex_in_progress_ = false;
                break;
            }
            default:
                std::cerr << "[SYSEX ERROR] Unknown SysEx7 status: " << (int)status << std::endl;
                break;
        }
        
        std::cout << "[SYSEX DATA] Current buffer (" << sysex_buffer_.size() << " bytes): ";
        for (auto byte : sysex_buffer_) {
            std::cout << std::hex << "0x" << (int)byte << " ";
        }
        std::cout << std::dec << std::endl;
    }
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


void KeyboardController::sendMidiCIDiscovery() {
    if (midiCIManager && midiCIManager->isInitialized()) {
        midiCIManager->sendDiscovery();
    }
}

std::vector<std::string> KeyboardController::getMidiCIDevices() {
    if (midiCIManager && midiCIManager->isInitialized()) {
        return midiCIManager->getDiscoveredDevices();
    }
    return {};
}

std::vector<MidiCIDeviceInfo> KeyboardController::getMidiCIDeviceDetails() {
    if (midiCIManager && midiCIManager->isInitialized()) {
        return midiCIManager->getDiscoveredDeviceDetails();
    }
    return {};
}

MidiCIDeviceInfo* KeyboardController::getMidiCIDeviceByMuid(uint32_t muid) {
    if (midiCIManager && midiCIManager->isInitialized()) {
        return midiCIManager->getDeviceByMuid(muid);
    }
    return nullptr;
}

bool KeyboardController::isMidiCIInitialized() const {
    return midiCIManager && midiCIManager->isInitialized();
}

uint32_t KeyboardController::getMidiCIMuid() const {
    if (midiCIManager) {
        return midiCIManager->getMuid();
    }
    return 0;
}

std::string KeyboardController::getMidiCIDeviceName() const {
    if (midiCIManager) {
        return midiCIManager->getDeviceName();
    }
    return "";
}

void KeyboardController::setMidiCIDevicesChangedCallback(std::function<void()> callback) {
    if (midiCIManager) {
        midiCIManager->setDevicesChangedCallback(callback);
    }
}

bool KeyboardController::hasValidMidiPair() const {
    return midiIn && midiIn->is_port_open() && midiOut && midiOut->is_port_open();
}

void KeyboardController::setMidiConnectionChangedCallback(std::function<void(bool)> callback) {
    midiConnectionChangedCallback = callback;
}

void KeyboardController::initializeMidiCI() {
    try {
        midiCIManager = std::make_unique<MidiCIManager>();
        
        // Set up logging callback
        midiCIManager->setLogCallback([](const std::string& message) {
            std::cout << message << std::endl;
        });
        
        // Set up SysEx sender callback BEFORE initialization
        midiCIManager->setSysExSender([this](uint8_t group, const std::vector<uint8_t>& data) -> bool {
            std::cout << "[SYSEX CALLBACK] External SysEx sender called with " << data.size() << " bytes" << std::endl;
            return sendSysExViaMidi(group, data);
        });
        
        // Initialize the MIDI-CI manager (will now use the SysEx sender)
        if (!midiCIManager->initialize()) {
            std::cerr << "Failed to initialize MIDI-CI manager" << std::endl;
            midiCIManager.reset();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error initializing MIDI-CI: " << e.what() << std::endl;
        midiCIManager.reset();
    }
}

void KeyboardController::processSysExForMidiCI(const std::vector<uint8_t>& sysex_data) {
    std::cout << "[MIDI-CI CHECK] Processing SysEx for MIDI-CI, size: " << sysex_data.size() << std::endl;
    
    if (midiCIManager && midiCIManager->isInitialized()) {
        // Check if this is a MIDI-CI message (starts with 0x7E for Universal Non-Real Time)
        if (!sysex_data.empty() && sysex_data[0] == 0xF0 && sysex_data.size() > 2 && sysex_data[1] == 0x7E) {
            std::cout << "[MIDI-CI DETECTED] Universal Non-Real Time SysEx (0x7E)" << std::endl;
            
            if (sysex_data.size() > 3) {
                std::cout << "[MIDI-CI INFO] Device ID: 0x" << std::hex << (int)sysex_data[2] << std::dec;
                if (sysex_data.size() > 4) {
                    std::cout << ", Sub-ID1: 0x" << std::hex << (int)sysex_data[3] << std::dec;
                    if (sysex_data[3] == 0x0D) {
                        std::cout << " (MIDI-CI)";
                    }
                }
                std::cout << std::endl;
            }
            
            // Strip F0 start and F7 end bytes - midicci expects payload only
            std::vector<uint8_t> payload_data;
            if (sysex_data.size() >= 2) {
                // Skip F0 at start, and F7 at end if present
                size_t start_idx = (sysex_data[0] == 0xF0) ? 1 : 0;
                size_t end_idx = sysex_data.size();
                if (end_idx > 0 && sysex_data[end_idx - 1] == 0xF7) {
                    end_idx--;
                }
                
                if (end_idx > start_idx) {
                    payload_data.assign(sysex_data.begin() + start_idx, sysex_data.begin() + end_idx);
                    
                    std::cout << "[MIDI-CI PAYLOAD] Stripped F0/F7, payload size: " << payload_data.size() << std::endl;
                    std::cout << "[MIDI-CI PAYLOAD] Data: ";
                    for (size_t i = 0; i < std::min(payload_data.size(), size_t(16)); i++) {
                        std::cout << std::hex << "0x" << (int)payload_data[i] << " ";
                    }
                    if (payload_data.size() > 16) std::cout << "...";
                    std::cout << std::dec << std::endl;
                    
                    midiCIManager->processMidi1SysEx(payload_data);
                } else {
                    std::cout << "[MIDI-CI ERROR] Invalid SysEx payload after stripping F0/F7" << std::endl;
                }
            }
        } else {
            std::cout << "[MIDI-CI SKIP] Not a MIDI-CI message (not 0xF0 0x7E)" << std::endl;
        }
    } else {
        std::cout << "[MIDI-CI SKIP] MIDI-CI Manager not initialized" << std::endl;
    }
}

bool KeyboardController::sendSysExViaMidi(uint8_t group, const std::vector<uint8_t>& data) {
    if (!midiOut || !initialized) {
        return false;
    }
    
    try {
        std::cout << "[SYSEX SEND] Sending " << data.size() << " bytes via UMP SYSEX7 using cmidi2" << std::endl;
        
        // Log the complete data being sent
        std::cout << "[SYSEX SEND] Complete data (" << data.size() << " bytes): ";
        for (size_t i = 0; i < data.size(); i++) {
            std::cout << std::hex << "0x" << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        // Use cmidi2 to convert SysEx to UMP SYSEX7 packets
        void* result = cmidi2_ump_sysex7_process(
            group,
            const_cast<void*>(static_cast<const void*>(data.data())),
            [](uint64_t umpData, void* context) -> void* {
                auto* controller = static_cast<KeyboardController*>(context);
                
                // Extract the two 32-bit words from the 64-bit UMP data
                uint32_t word0 = static_cast<uint32_t>(umpData >> 32);
                uint32_t word1 = static_cast<uint32_t>(umpData & 0xFFFFFFFF);
                
                // Create UMP packet and send it
                libremidi::ump packet(word0, word1, 0, 0);
                
                std::cout << "[SYSEX SEND] UMP packet: 0x" << std::hex << word0 << " 0x" << word1 << std::dec << std::endl;
                
                try {
                    controller->midiOut->send_ump(packet);
                } catch (const std::exception& e) {
                    std::cerr << "Failed to send UMP SYSEX7 packet: " << e.what() << std::endl;
                    return const_cast<void*>(static_cast<const void*>(&e)); // Return error
                }
                
                return nullptr; // Success
            },
            this
        );
        
        if (result != nullptr) {
            std::cerr << "[SYSEX SEND] cmidi2_ump_sysex7_process failed" << std::endl;
            return false;
        }
        
        std::cout << "[SYSEX SEND] UMP SYSEX7 packets sent successfully using cmidi2" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error sending SysEx via UMP: " << e.what() << std::endl;
        return false;
    }
}

void KeyboardController::updateUIConnectionState() {
    bool currentConnectionState = hasValidMidiPair();
    
    if (currentConnectionState != previousConnectionState) {
        previousConnectionState = currentConnectionState;
        
        if (midiConnectionChangedCallback) {
            midiConnectionChangedCallback(currentConnectionState);
        }
        
        std::cout << "MIDI connection pair state changed: " << (currentConnectionState ? "CONNECTED" : "DISCONNECTED") << std::endl;
    }
}