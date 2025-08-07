#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtCore/QSignalMapper>
#include <QtCore/QTimer>
#include <functional>
#include "midi_ci_manager.h"

class PianoKey;

class KeyboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit KeyboardWidget(QWidget* parent = nullptr);
    
    void setKeyPressedCallback(std::function<void(int)> callback);
    void setKeyReleasedCallback(std::function<void(int)> callback);
    void setDeviceRefreshCallback(std::function<void()> callback);
    
    void updateMidiDevices(const std::vector<std::pair<std::string, std::string>>& inputDevices,
                          const std::vector<std::pair<std::string, std::string>>& outputDevices);
    
    // MIDI-CI UI methods
    void updateMidiCIStatus(bool initialized, uint32_t muid, const std::string& deviceName);
    void updateMidiCIDevices(const std::vector<MidiCIDeviceInfo>& discoveredDevices);
    void setMidiCIDiscoveryCallback(std::function<void()> callback);
    void setMidiCIUpdateCallback(std::function<void()> callback);
    void setMidiCIDeviceProvider(std::function<MidiCIDeviceInfo*(uint32_t)> provider);

signals:
    void midiInputDeviceChanged(const QString& deviceId);
    void midiOutputDeviceChanged(const QString& deviceId);

private slots:
    void onKeyPressed(int note);
    void onKeyReleased(int note);
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void refreshDevices();
    void sendMidiCIDiscovery();
    void updateMidiCIPeriodically();
    void onMidiCIDeviceSelected(int index);

private:
    void setupUI();
    void setupKeyboard();
    void setupDeviceSelectors();
    void setupMidiCIControls();
    QWidget* createKeyboardWidget();
    
    std::function<void(int)> keyPressedCallback;
    std::function<void(int)> keyReleasedCallback;
    std::function<void()> deviceRefreshCallback;
    std::function<void()> midiCIDiscoveryCallback;
    std::function<void()> midiCIUpdateCallback;
    std::function<MidiCIDeviceInfo*(uint32_t)> midiCIDeviceProvider;
    
    QVBoxLayout* mainLayout;
    QWidget* keyboardWidget;
    QGroupBox* deviceGroup;
    QHBoxLayout* deviceLayout;
    QComboBox* inputDeviceCombo;
    QComboBox* outputDeviceCombo;
    QPushButton* refreshButton;
    QHBoxLayout* controlsLayout;
    QLabel* titleLabel;
    QLabel* velocityLabel;
    QProgressBar* velocityBar;
    
    // MIDI-CI UI elements
    QGroupBox* midiCIGroup;
    QLabel* midiCIStatusLabel;
    QLabel* midiCIMuidLabel;
    QLabel* midiCIDeviceNameLabel;
    QPushButton* midiCIDiscoveryButton;
    QComboBox* midiCIDeviceCombo;
    QLabel* midiCISelectedDeviceInfo;
    
    QList<PianoKey*> whiteKeys;
    QList<PianoKey*> blackKeys;
    
    QSignalMapper* pressMapper;
    QSignalMapper* releaseMapper;
    QTimer* midiCIUpdateTimer;
};