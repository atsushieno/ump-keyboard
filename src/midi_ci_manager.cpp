#include "midi_ci_manager.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>

using namespace midicci::commonproperties;

MidiCIManager::MidiCIManager() 
    : muid_(0), initialized_(false) {
}

MidiCIManager::~MidiCIManager() {
    shutdown();
}

bool MidiCIManager::initialize(uint32_t muid) {
    try {
        if (initialized_) {
            std::cout << "[MIDI-CI] MIDI-CI Manager already initialized" << std::endl;
            return true;
        }
        
        // Generate MUID if not provided
        if (muid == 0) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dis(1, 0xFFFFFF); // Avoid 0 and reserved values
            muid_ = dis(gen) & 0x7F7F7F7F;
        } else {
            muid_ = muid;
        }
        
        // Create device configuration
        setupDeviceConfiguration();
        
        // Create MIDI-CI device
        device_ = std::make_unique<midicci::MidiCIDevice>(
            muid_, 
            *config_,
            [this](const std::string& message, bool is_outgoing) {
                // Use our log method with proper parameter semantics
                log(message, is_outgoing);
            }
        );
        
        // Setup callbacks
        setupCallbacks();
        
        // Set up SysEx sender if already provided
        if (sysex_sender_) {
            device_->set_sysex_sender([this](uint8_t group, const std::vector<uint8_t>& data) -> bool {
                if (sysex_sender_) {
                    return sysex_sender_(group, data);
                }
                return false;
            });
        }
        
        initialized_ = true;
        std::cout << "[MIDI-CI] MIDI-CI Manager initialized with MUID: 0x" 
                  << std::hex << muid_ << std::dec << " (" << muid_ << ")" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Failed to initialize MIDI-CI Manager: " << e.what() << std::endl;
        return false;
    }
}

void MidiCIManager::shutdown() {
    if (!initialized_) return;
    
    try {
        device_.reset();
        config_.reset();
        initialized_ = false;
        std::cout << "[MIDI-CI] MIDI-CI Manager shutdown complete" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Error during MIDI-CI Manager shutdown: " << e.what() << std::endl;
    }
}

void MidiCIManager::processMidi1SysEx(const std::vector<uint8_t>& sysex_data) {
    if (!initialized_ || !device_) return;
    
    std::cout << "[MIDICCI] Processing MIDI 1.0 SysEx, size: " << sysex_data.size() << std::endl;
    std::cout << "[MIDICCI] SysEx data: ";
    for (size_t i = 0; i < std::min(sysex_data.size(), size_t(16)); i++) {
        std::cout << std::hex << "0x" << (int)sysex_data[i] << " ";
    }
    if (sysex_data.size() > 16) std::cout << "... (truncated)";
    std::cout << std::dec << std::endl;
    
    try {
        // Process MIDI 1.0 SysEx data through MIDI-CI device
        device_->processInput(0, sysex_data); // Use group 0 for MIDI 1.0
        std::cout << "[MIDICCI] SysEx processed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Error processing MIDI 1.0 SysEx: " << e.what() << std::endl;
    }
}

void MidiCIManager::processUmpSysEx(uint8_t group, const std::vector<uint8_t>& sysex_data) {
    if (!initialized_ || !device_) return;
    
    try {
        // Process UMP SysEx data through MIDI-CI device
        device_->processInput(group, sysex_data);
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Error processing UMP SysEx: " << e.what() << std::endl;
    }
}

void MidiCIManager::sendDiscovery() {
    if (!initialized_ || !device_) {
        std::cerr << "[MIDI-CI ERROR] Cannot send discovery - MIDI-CI Manager not initialized" << std::endl;
        return;
    }
    
    std::cout << "[DISCOVERY SEND] Sending MIDI-CI Discovery inquiry..." << std::endl;
    
    try {
        device_->sendDiscovery();
        log("Discovery inquiry sent", true); // outgoing message
        std::cout << "[DISCOVERY SEND] Discovery inquiry sent successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Error sending discovery: " << e.what() << std::endl;
    }
}

std::vector<std::string> MidiCIManager::getDiscoveredDevices() const {
    std::vector<std::string> devices;
    
    for (const auto& device_info : discovered_devices_) {
        devices.push_back(device_info.getDisplayName());
    }
    
    return devices;
}

std::vector<MidiCIDeviceInfo> MidiCIManager::getDiscoveredDeviceDetails() const {
    return discovered_devices_;
}

MidiCIDeviceInfo* MidiCIManager::getDeviceByMuid(uint32_t muid) {
    for (auto& device_info : discovered_devices_) {
        if (device_info.muid == muid) {
            return &device_info;
        }
    }
    return nullptr;
}

void MidiCIManager::setSysExSender(SysExSender sender) {
    sysex_sender_ = sender;
    
    if (initialized_ && device_ && sender) {
        // Set up the CI output sender for the device
        device_->set_sysex_sender([this](uint8_t group, const std::vector<uint8_t>& data) -> bool {
            if (sysex_sender_) {
                return sysex_sender_(group, data);
            }
            return false;
        });
    }
}

void MidiCIManager::setLogCallback(LogCallback callback) {
    log_callback_ = callback;
}

void MidiCIManager::setDevicesChangedCallback(DevicesChangedCallback callback) {
    devices_changed_callback_ = callback;
}

void MidiCIManager::setPropertiesChangedCallback(std::function<void(uint32_t)> callback) {
    properties_changed_callback_ = callback;
}

uint32_t MidiCIManager::getMuid() const {
    return muid_;
}

std::string MidiCIManager::getDeviceName() const {
    if (!initialized_ || !config_) return "";
    return config_->device_info.model;
}

bool MidiCIManager::isInitialized() const {
    return initialized_;
}

void MidiCIManager::setupDeviceConfiguration() {
    config_ = std::make_unique<midicci::MidiCIDeviceConfiguration>();
    
    // Set up basic device information using the correct fields
    config_->device_info.manufacturer_id = 0x654321;  // Use midicci default for now
    config_->device_info.family_id = 0x4321;
    config_->device_info.model_id = 0x765;
    config_->device_info.version_id = 0x00000001;
    config_->device_info.manufacturer = "atsushieno";
    config_->device_info.family = "UMP";
    config_->device_info.model = "UMP Keyboard";
    config_->device_info.version = "1.0";
    config_->device_info.serial_number = "UMP-KB-001";
    
    // Enable basic capabilities
    config_->capability_inquiry_supported = static_cast<uint8_t>(midicci::MidiCISupportedCategories::THREE_P);
    config_->auto_send_endpoint_inquiry = true;
    config_->auto_send_profile_inquiry = true;
    config_->auto_send_property_exchange_capabilities_inquiry = true;
    config_->auto_send_process_inquiry = true;
    config_->auto_send_get_resource_list = true;
    config_->auto_send_get_device_info = true;
    
    // Add basic General MIDI profile
    std::vector<uint8_t> gm_profile_data{0x7E, 0x00, 0x00, 0x00, 0x01}; // General MIDI Level 1
    midicci::MidiCIProfileId gm_profile_id(gm_profile_data);
    midicci::MidiCIProfile gm_profile(gm_profile_id, 0, 0, false, 16); // group 0, address 0, not enabled initially, 16 channels
    config_->local_profiles.push_back(gm_profile);
}

void MidiCIManager::setupCallbacks() {
    if (!device_) return;
    
    // Set up message callback for outgoing messages
    device_->set_message_callback([this](const midicci::Message& message) {
        std::string msgTypeName;
        switch(message.get_type()) {
            case midicci::MessageType::DiscoveryInquiry: msgTypeName = "DiscoveryInquiry"; break;
            case midicci::MessageType::DiscoveryReply: msgTypeName = "DiscoveryReply"; break;
            case midicci::MessageType::GetPropertyData: msgTypeName = "GetPropertyData"; break;
            case midicci::MessageType::GetPropertyDataReply: msgTypeName = "GetPropertyDataReply"; break;
            default: msgTypeName = "Unknown(" + std::to_string(static_cast<int>(message.get_type())) + ")"; break;
        }
        log("MIDI-CI Message sent: " + msgTypeName, true); // outgoing
        std::cout << "[MIDI-CI SENT] " << msgTypeName << " to MUID: 0x" << std::hex << message.get_destination_muid() << std::dec << std::endl;
    });
    
    // Set up message received callback
    device_->set_message_received_callback([this](const midicci::Message& message) {
        std::cout << "[UMP-KEYBOARD RECV] Message type: " << static_cast<int>(message.get_type()) << std::endl;
        log("MIDI-CI Message received: " + std::to_string(static_cast<int>(message.get_type())), false); // incoming
        
        // Handle GetPropertyDataReply messages for property retrieval
        if (message.get_type() == midicci::MessageType::GetPropertyDataReply) {
            std::cout << "[PROPERTY REPLY] Processing GetPropertyDataReply message" << std::endl;
            
            try {
                const auto* property_reply = dynamic_cast<const midicci::GetPropertyDataReply*>(&message);
                if (property_reply) {
                    handleGetPropertyDataReply(*property_reply);
                } else {
                    std::cout << "[PROPERTY REPLY ERROR] Failed to cast to GetPropertyDataReply" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[MIDI-CI ERROR] Error processing property data reply: " << e.what() << std::endl;
            }
        }
        // Handle Discovery Reply messages to populate device list
        else if (message.get_type() == midicci::MessageType::DiscoveryReply) {
            std::cout << "[DISCOVERY REPLY] Processing discovery reply message" << std::endl;
            
            // Try to cast to DiscoveryReply to get device details
            try {
                const auto* discovery_reply = dynamic_cast<const midicci::DiscoveryReply*>(&message);
                if (discovery_reply) {
                    uint32_t source_muid = discovery_reply->get_source_muid();
                    const auto& device_details = discovery_reply->get_device_details();
                    
                    std::cout << "[DISCOVERY REPLY] Source MUID: 0x" << std::hex << source_muid << std::dec << " (" << source_muid << ")" << std::endl;
                    std::cout << "[DISCOVERY REPLY] Device details - Manufacturer: 0x" << std::hex << device_details.manufacturer << std::dec 
                              << ", Family: 0x" << std::hex << device_details.family << std::dec
                              << ", Model: 0x" << std::hex << device_details.modelNumber << std::dec << std::endl;
                    
                    // Check if we already have this device
                    bool found = false;
                    for (auto& existing : discovered_devices_) {
                        if (existing.muid == source_muid) {
                            std::cout << "[DISCOVERY REPLY] Device already exists in list" << std::endl;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        std::cout << "[DISCOVERY REPLY] Adding new device to list" << std::endl;
                        
                        // Add new device to our list
                        MidiCIDeviceInfo new_device(
                            source_muid,
                            "MIDI-CI Device", // placeholder name
                            "Unknown", // Will be populated later if available
                            "MIDI-CI Device", // placeholder model
                            "1.0", // placeholder version
                            0, // features placeholder
                            4096 // default sysex size
                        );
                        discovered_devices_.push_back(new_device);
                        
                        log("New MIDI-CI device discovered: MUID 0x" + std::to_string(source_muid), false); // incoming discovery
                        std::cout << "[DISCOVERY REPLY] Device added, total devices: " << discovered_devices_.size() << std::endl;
                        
                        // Notify about device list change
                        if (devices_changed_callback_) {
                            std::cout << "[DISCOVERY REPLY] Calling devices changed callback" << std::endl;
                            devices_changed_callback_();
                        } else {
                            std::cout << "[DISCOVERY REPLY] No devices changed callback set" << std::endl;
                        }
                    }
                } else {
                    std::cout << "[DISCOVERY REPLY ERROR] Failed to cast to DiscoveryReply" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[MIDI-CI ERROR] Error processing discovery reply: " << e.what() << std::endl;
            }
        } else {
            std::cout << "[UMP-KEYBOARD RECV] Message type " << static_cast<int>(message.get_type()) << " is not DiscoveryReply (" << static_cast<int>(midicci::MessageType::DiscoveryReply) << ")" << std::endl;
        }
    });
    
    // Set up connections changed callback
    device_->set_connections_changed_callback([this]() {
        log("MIDI-CI Connections changed", false); // incoming change notification
        if (devices_changed_callback_) {
            devices_changed_callback_();
        }
    });
}

void MidiCIManager::log(const std::string& message, bool is_outgoing) {
    std::string prefix = is_outgoing ? "[MIDI-CI OUT] " : "[MIDI-CI IN] ";
    std::string full_message = prefix + message;
    
    if (log_callback_) {
        log_callback_(full_message);
    } else {
        std::cout << full_message << std::endl;
    }
}

// Property management methods
void MidiCIManager::requestAllCtrlList(uint32_t muid) {
    if (!initialized_ || !device_) {
        std::cerr << "[MIDI-CI ERROR] Cannot request properties - not initialized" << std::endl;
        return;
    }
    
    // Check if we already requested this property
    auto& state = property_request_states_[muid];
    if (state.ctrl_list_requested) {
        if (state.ctrl_list_received) {
            std::cout << "[PROPERTY REQUEST] AllCtrlList already received for MUID: 0x" << std::hex << muid << std::dec << std::endl;
        } else {
            std::cout << "[PROPERTY REQUEST] AllCtrlList request already sent for MUID: 0x" << std::hex << muid << std::dec << ", waiting for reply..." << std::endl;
        }
        return;
    }
    
    std::cout << "[PROPERTY REQUEST] Requesting AllCtrlList from MUID: 0x" << std::hex << muid << std::dec << std::endl;
    
    try {
        // Create Common struct for the message
        midicci::Common common(muid_, muid, 0, 0);
        
        // Create a GetPropertyData message
        midicci::GetPropertyData request(common, 1, StandardPropertyNames::ALL_CTRL_LIST, "");
        
        // Send the message using the device's messenger
        device_->get_messenger().send(request);
        
        // Mark as requested
        state.ctrl_list_requested = true;
        
        log("Sent AllCtrlList property request for MUID 0x" + std::to_string(muid), true);
        std::cout << "[PROPERTY REQUEST] AllCtrlList request sent successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Failed to request AllCtrlList: " << e.what() << std::endl;
        // Reset request state on error
        state.ctrl_list_requested = false;
    }
}

void MidiCIManager::requestProgramList(uint32_t muid) {
    if (!initialized_ || !device_) {
        std::cerr << "[MIDI-CI ERROR] Cannot request properties - not initialized" << std::endl;
        return;
    }
    
    // Check if we already requested this property
    auto& state = property_request_states_[muid];
    if (state.program_list_requested) {
        if (state.program_list_received) {
            std::cout << "[PROPERTY REQUEST] ProgramList already received for MUID: 0x" << std::hex << muid << std::dec << std::endl;
        } else {
            std::cout << "[PROPERTY REQUEST] ProgramList request already sent for MUID: 0x" << std::hex << muid << std::dec << ", waiting for reply..." << std::endl;
        }
        return;
    }
    
    std::cout << "[PROPERTY REQUEST] Requesting ProgramList from MUID: 0x" << std::hex << muid << std::dec << std::endl;
    
    try {
        // Create Common struct for the message
        midicci::Common common(muid_, muid, 0, 0);
        
        // Create a GetPropertyData message
        midicci::GetPropertyData request(common, 2, StandardPropertyNames::PROGRAM_LIST, "");
        
        // Send the message using the device's messenger
        device_->get_messenger().send(request);
        
        // Mark as requested
        state.program_list_requested = true;
        
        log("Sent ProgramList property request for MUID 0x" + std::to_string(muid), true);
        std::cout << "[PROPERTY REQUEST] ProgramList request sent successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Failed to request ProgramList: " << e.what() << std::endl;
        // Reset request state on error
        state.program_list_requested = false;
    }
}

std::vector<midicci::commonproperties::MidiCIControl> MidiCIManager::getAllCtrlList(uint32_t muid) {
    auto it = device_ctrl_lists_.find(muid);
    if (it != device_ctrl_lists_.end()) {
        return it->second;
    }
    return {};
}

std::vector<midicci::commonproperties::MidiCIProgram> MidiCIManager::getProgramList(uint32_t muid) {
    auto it = device_program_lists_.find(muid);
    if (it != device_program_lists_.end()) {
        return it->second;
    }
    return {};
}

void MidiCIManager::setupPropertyClientForDevice(uint32_t muid) {
    // This method is now simplified since we're using direct message sending
    // Just ensure we have storage slots for this device
    if (device_ctrl_lists_.find(muid) == device_ctrl_lists_.end()) {
        device_ctrl_lists_[muid] = {};
    }
    if (device_program_lists_.find(muid) == device_program_lists_.end()) {
        device_program_lists_[muid] = {};
    }
}

void MidiCIManager::handleGetPropertyDataReply(const midicci::GetPropertyDataReply& msg) {
    uint32_t source_muid = msg.get_source_muid();
    
    std::cout << "[PROPERTY REPLY] Received property data reply from MUID: 0x" << std::hex << source_muid << std::dec << std::endl;
    
    try {
        // Get the property data from the message body
        auto data = msg.get_body();
        uint8_t request_id = msg.get_request_id();
        auto header = msg.get_header();
        
        std::cout << "[PROPERTY DEBUG] Request ID: " << static_cast<int>(request_id) << std::endl;
        std::cout << "[PROPERTY DEBUG] Header size: " << header.size() << " bytes" << std::endl;
        std::cout << "[PROPERTY DEBUG] Body size: " << data.size() << " bytes" << std::endl;
        
        // Print first part of body data for debugging
        if (!data.empty()) {
            std::cout << "[PROPERTY DEBUG] Body data (first 100 bytes): ";
            for (size_t i = 0; i < std::min(data.size(), size_t(100)); i++) {
                if (data[i] >= 32 && data[i] < 127) {
                    std::cout << static_cast<char>(data[i]);
                } else {
                    std::cout << "\\x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]) << std::dec;
                }
            }
            if (data.size() > 100) std::cout << " ... (truncated)";
            std::cout << std::endl;
        }
        
        // Check if this is AllCtrlList (request_id = 1) or ProgramList (request_id = 2)
        bool is_ctrl_list = (request_id == 1);
        bool is_program_list = (request_id == 2);
        
        if (is_ctrl_list) {
            std::cout << "[PROPERTY PARSE] Attempting to parse as AllCtrlList" << std::endl;
            try {
                auto controls = StandardProperties::parseControlList(data);
                std::cout << "[PROPERTY PARSED] Successfully parsed " << controls.size() << " controls for MUID 0x" << std::hex << source_muid << std::dec << std::endl;
                
                device_ctrl_lists_[source_muid] = controls;
                property_request_states_[source_muid].ctrl_list_received = true;
                
                if (properties_changed_callback_) {
                    properties_changed_callback_(source_muid);
                }
                return;
            } catch (const std::exception& e) {
                std::cerr << "[PROPERTY ERROR] Failed to parse control list: " << e.what() << std::endl;
            }
        } else if (is_program_list) {
            std::cout << "[PROPERTY PARSE] Attempting to parse as ProgramList" << std::endl;
            try {
                auto programs = StandardProperties::parseProgramList(data);
                std::cout << "[PROPERTY PARSED] Successfully parsed " << programs.size() << " programs for MUID 0x" << std::hex << source_muid << std::dec << std::endl;
                
                device_program_lists_[source_muid] = programs;
                property_request_states_[source_muid].program_list_received = true;
                
                if (properties_changed_callback_) {
                    properties_changed_callback_(source_muid);
                }
                return;
            } catch (const std::exception& e) {
                std::cerr << "[PROPERTY ERROR] Failed to parse program list: " << e.what() << std::endl;
            }
        } else {
            std::cout << "[PROPERTY WARNING] Unknown property type with request ID: " << static_cast<int>(request_id) << std::endl;
        }
        
        // If we get here, parsing failed
        std::cout << "[PROPERTY WARNING] Could not parse property data" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[MIDI-CI ERROR] Error handling property data reply: " << e.what() << std::endl;
    }
}

void MidiCIManager::resetPropertyRequestState(uint32_t muid) {
    std::cout << "[PROPERTY RESET] Resetting property request state for MUID: 0x" << std::hex << muid << std::dec << std::endl;
    property_request_states_[muid] = PropertyRequestState{}; // Reset to default state
    device_ctrl_lists_[muid].clear();
    device_program_lists_[muid].clear();
}