#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <cstdint>
#include <map>
#include <midicci/midicci.hpp>

struct MidiCIDeviceInfo {
    uint32_t muid;
    std::string device_name;
    std::string manufacturer;
    std::string model;
    std::string version;
    uint8_t supported_features;
    uint32_t max_sysex_size;
    
    MidiCIDeviceInfo(uint32_t m, const std::string& name, const std::string& mfg, const std::string& mod, 
                     const std::string& ver, uint8_t features, uint32_t sysex_size)
        : muid(m), device_name(name), manufacturer(mfg), model(mod), version(ver), 
          supported_features(features), max_sysex_size(sysex_size) {}
    
    std::string getDisplayName() const {
        return model + " (" + manufacturer + ")";
    }
    
    std::string getFullInfo() const {
        return "MUID: 0x" + std::to_string(muid) + ", " + 
               manufacturer + " " + model + " v" + version;
    }
};

class MidiCIManager {
public:
    using LogCallback = std::function<void(const std::string&)>;
    using SysExSender = std::function<bool(uint8_t group, const std::vector<uint8_t>& data)>;
    using DevicesChangedCallback = std::function<void()>;
    
    MidiCIManager();
    ~MidiCIManager();
    
    // Initialization and cleanup
    bool initialize(uint32_t muid = 0);
    void shutdown();
    
    // MIDI message processing
    void processMidi1SysEx(const std::vector<uint8_t>& sysex_data);
    void processUmpSysEx(uint8_t group, const std::vector<uint8_t>& sysex_data);
    
    // Device management
    void sendDiscovery();
    std::vector<std::string> getDiscoveredDevices() const;
    std::vector<MidiCIDeviceInfo> getDiscoveredDeviceDetails() const;
    MidiCIDeviceInfo* getDeviceByMuid(uint32_t muid);
    
    // Configuration
    void setSysExSender(SysExSender sender);
    void setLogCallback(LogCallback callback);
    void setDevicesChangedCallback(DevicesChangedCallback callback);
    
    // Device information
    uint32_t getMuid() const;
    std::string getDeviceName() const;
    bool isInitialized() const;
    
    // Property management
    void requestAllCtrlList(uint32_t muid);
    void requestProgramList(uint32_t muid);
    std::vector<midicci::commonproperties::MidiCIControl> getAllCtrlList(uint32_t muid);
    std::vector<midicci::commonproperties::MidiCIProgram> getProgramList(uint32_t muid);
    void setPropertiesChangedCallback(std::function<void(uint32_t)> callback);
    void resetPropertyRequestState(uint32_t muid);
    
private:
    std::unique_ptr<midicci::MidiCIDevice> device_;
    std::unique_ptr<midicci::MidiCIDeviceConfiguration> config_;
    
    SysExSender sysex_sender_;
    LogCallback log_callback_;
    DevicesChangedCallback devices_changed_callback_;
    std::function<void(uint32_t)> properties_changed_callback_;
    uint32_t muid_;
    bool initialized_;
    
    std::vector<MidiCIDeviceInfo> discovered_devices_;
    
    // Property storage for each device
    std::map<uint32_t, std::vector<midicci::commonproperties::MidiCIControl>> device_ctrl_lists_;
    std::map<uint32_t, std::vector<midicci::commonproperties::MidiCIProgram>> device_program_lists_;
    std::map<uint32_t, std::unique_ptr<midicci::PropertyClientFacade>> property_clients_;
    
    // Request tracking to prevent duplicate requests
    struct PropertyRequestState {
        bool ctrl_list_requested = false;
        bool program_list_requested = false;
        bool ctrl_list_received = false;
        bool program_list_received = false;
    };
    std::map<uint32_t, PropertyRequestState> property_request_states_;
    
    // Device configuration helpers
    void setupDeviceConfiguration();
    void setupCallbacks();
    
    // Property management helpers
    void setupPropertyClientForDevice(uint32_t muid);
    void handleGetPropertyDataReply(const midicci::GetPropertyDataReply& msg);
    
    // Logging helper
    void log(const std::string& message, bool is_outgoing = false);
};