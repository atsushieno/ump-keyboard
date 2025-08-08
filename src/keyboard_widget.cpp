#include "keyboard_widget.h"
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QGridLayout>
#include <QtCore/QDebug>

class PianoKey : public QPushButton {
    Q_OBJECT
public:
    PianoKey(int note, bool isBlack = false, QWidget* parent = nullptr) 
        : QPushButton(parent), noteValue(note), isBlackKey(isBlack) {
        setupKey();
    }
    
    int getNote() const { return noteValue; }
    bool isBlack() const { return isBlackKey; }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        QPushButton::mousePressEvent(event);
        emit keyPressed(noteValue);
    }
    
    void mouseReleaseEvent(QMouseEvent* event) override {
        QPushButton::mouseReleaseEvent(event);
        emit keyReleased(noteValue);
    }

signals:
    void keyPressed(int note);
    void keyReleased(int note);

private:
    void setupKey() {
        if (isBlackKey) {
            setFixedSize(30, 80);
            setStyleSheet(
                "QPushButton {"
                "  background-color: #1a1a1a;"
                "  border: 1px solid #333;"
                "  border-radius: 4px;"
                "  color: white;"
                "}"
                "QPushButton:pressed {"
                "  background-color: #404040;"
                "}"
            );
        } else {
            setFixedSize(50, 120);
            setStyleSheet(
                "QPushButton {"
                "  background-color: white;"
                "  border: 1px solid #333;"
                "  border-radius: 4px;"
                "  color: black;"
                "}"
                "QPushButton:pressed {"
                "  background-color: #f0f0f0;"
                "}"
            );
        }
    }
    
    int noteValue;
    bool isBlackKey;
};

KeyboardWidget::KeyboardWidget(QWidget* parent) 
    : QWidget(parent), pressMapper(new QSignalMapper(this)), releaseMapper(new QSignalMapper(this)) {
    setupUI();
    
    connect(pressMapper, &QSignalMapper::mappedInt, this, &KeyboardWidget::onKeyPressed);
    connect(releaseMapper, &QSignalMapper::mappedInt, this, &KeyboardWidget::onKeyReleased);
    
    // Set up periodic MIDI-CI updates
    midiCIUpdateTimer = new QTimer(this);
    connect(midiCIUpdateTimer, &QTimer::timeout, this, &KeyboardWidget::updateMidiCIPeriodically);
    midiCIUpdateTimer->start(2000); // Update every 2 seconds
}

void KeyboardWidget::setupUI() {
    setFixedSize(900, 400);
    setWindowTitle("UMP Keyboard - MIDI 2.0 Virtual Piano");
    
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // Title
    titleLabel = new QLabel("MIDI 2.0 Virtual Keyboard");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel);
    
    // Device selectors
    setupDeviceSelectors();
    
    // MIDI-CI controls
    setupMidiCIControls();
    
    // Keyboard
    setupKeyboard();
    
    // Controls
    controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(10);
    
    velocityLabel = new QLabel("Velocity:");
    velocityLabel->setAlignment(Qt::AlignVCenter);
    controlsLayout->addWidget(velocityLabel);
    
    velocityBar = new QProgressBar();
    velocityBar->setFixedSize(200, 20);
    velocityBar->setValue(80);
    velocityBar->setStyleSheet(
        "QProgressBar {"
        "  border: 1px solid gray;"
        "  background-color: lightgray;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #4CAF50;"
        "}"
    );
    controlsLayout->addWidget(velocityBar);
    
    controlsLayout->addStretch();
    mainLayout->addLayout(controlsLayout);
}

void KeyboardWidget::setupDeviceSelectors() {
    deviceGroup = new QGroupBox("MIDI 2.0 Devices");
    deviceLayout = new QHBoxLayout(deviceGroup);
    
    // Input device selector
    QLabel* inputLabel = new QLabel("Input:");
    inputDeviceCombo = new QComboBox();
    inputDeviceCombo->setMinimumWidth(200);
    connect(inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KeyboardWidget::onInputDeviceChanged);
    
    // Output device selector
    QLabel* outputLabel = new QLabel("Output:");
    outputDeviceCombo = new QComboBox();
    outputDeviceCombo->setMinimumWidth(200);
    connect(outputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KeyboardWidget::onOutputDeviceChanged);
    
    // Refresh button
    refreshButton = new QPushButton("Refresh");
    refreshButton->setMaximumWidth(80);
    connect(refreshButton, &QPushButton::clicked, this, &KeyboardWidget::refreshDevices);
    
    deviceLayout->addWidget(inputLabel);
    deviceLayout->addWidget(inputDeviceCombo);
    deviceLayout->addSpacing(20);
    deviceLayout->addWidget(outputLabel);
    deviceLayout->addWidget(outputDeviceCombo);
    deviceLayout->addWidget(refreshButton);
    deviceLayout->addStretch();
    
    mainLayout->addWidget(deviceGroup);
}

void KeyboardWidget::setupKeyboard() {
    keyboardWidget = createKeyboardWidget();
    mainLayout->addWidget(keyboardWidget);
}

QWidget* KeyboardWidget::createKeyboardWidget() {
    QWidget* container = new QWidget();
    container->setFixedSize(850, 140);
    
    // Create all keys
    struct KeyInfo {
        int note;
        bool isBlack;
        QString label;
        int whiteKeyIndex; // For positioning black keys
    };
    
    // Define two octaves of keys (C4 to B5)
    std::vector<KeyInfo> keyInfos = {
        {60, false, "C", 0},   // C4
        {61, true, "", 0},     // C#4 (between C and D)
        {62, false, "D", 1},   // D4
        {63, true, "", 1},     // D#4 (between D and E)
        {64, false, "E", 2},   // E4
        {65, false, "F", 3},   // F4
        {66, true, "", 3},     // F#4 (between F and G)
        {67, false, "G", 4},   // G4
        {68, true, "", 4},     // G#4 (between G and A)
        {69, false, "A", 5},   // A4
        {70, true, "", 5},     // A#4 (between A and B)
        {71, false, "B", 6},   // B4
        {72, false, "C", 7},   // C5
        {73, true, "", 7},     // C#5
        {74, false, "D", 8},   // D5
        {75, true, "", 8},     // D#5
        {76, false, "E", 9},   // E5
        {77, false, "F", 10},  // F5
        {78, true, "", 10},    // F#5
        {79, false, "G", 11},  // G5
        {80, true, "", 11},    // G#5
        {81, false, "A", 12},  // A5
        {82, true, "", 12},    // A#5
        {83, false, "B", 13},  // B5
    };
    
    // Create white keys first
    for (const auto& keyInfo : keyInfos) {
        if (!keyInfo.isBlack) {
            PianoKey* key = new PianoKey(keyInfo.note, false, container);
            key->setText(keyInfo.label);
            key->move(keyInfo.whiteKeyIndex * 52, 20);
            
            connect(key, &PianoKey::keyPressed, pressMapper, QOverload<>::of(&QSignalMapper::map));
            connect(key, &PianoKey::keyReleased, releaseMapper, QOverload<>::of(&QSignalMapper::map));
            pressMapper->setMapping(key, keyInfo.note);
            releaseMapper->setMapping(key, keyInfo.note);
            
            whiteKeys.append(key);
        }
    }
    
    // Create black keys on top
    for (const auto& keyInfo : keyInfos) {
        if (keyInfo.isBlack) {
            PianoKey* key = new PianoKey(keyInfo.note, true, container);
            
            // Position black keys between white keys
            int xPos = keyInfo.whiteKeyIndex * 52 + 37; // 52 is white key width + spacing, 37 centers the black key
            key->move(xPos, 20);
            key->raise(); // Bring black keys to front
            
            connect(key, &PianoKey::keyPressed, pressMapper, QOverload<>::of(&QSignalMapper::map));
            connect(key, &PianoKey::keyReleased, releaseMapper, QOverload<>::of(&QSignalMapper::map));
            pressMapper->setMapping(key, keyInfo.note);
            releaseMapper->setMapping(key, keyInfo.note);
            
            blackKeys.append(key);
        }
    }
    
    return container;
}

void KeyboardWidget::setKeyPressedCallback(std::function<void(int)> callback) {
    keyPressedCallback = callback;
}

void KeyboardWidget::setKeyReleasedCallback(std::function<void(int)> callback) {
    keyReleasedCallback = callback;
}

void KeyboardWidget::setDeviceRefreshCallback(std::function<void()> callback) {
    deviceRefreshCallback = callback;
}

void KeyboardWidget::updateMidiDevices(
    const std::vector<std::pair<std::string, std::string>>& inputDevices,
    const std::vector<std::pair<std::string, std::string>>& outputDevices) {
    
    // Update input devices
    inputDeviceCombo->clear();
    inputDeviceCombo->addItem("No Input Device", "");
    for (const auto& device : inputDevices) {
        inputDeviceCombo->addItem(QString::fromStdString(device.second), 
                                QString::fromStdString(device.first));
    }
    
    // Update output devices
    outputDeviceCombo->clear();
    outputDeviceCombo->addItem("No Output Device", "");
    for (const auto& device : outputDevices) {
        outputDeviceCombo->addItem(QString::fromStdString(device.second), 
                                 QString::fromStdString(device.first));
    }
}

void KeyboardWidget::onKeyPressed(int note) {
    if (keyPressedCallback) {
        keyPressedCallback(note);
    }
}

void KeyboardWidget::onKeyReleased(int note) {
    if (keyReleasedCallback) {
        keyReleasedCallback(note);
    }
}

void KeyboardWidget::onInputDeviceChanged(int index) {
    if (index >= 0) {
        QString deviceId = inputDeviceCombo->itemData(index).toString();
        emit midiInputDeviceChanged(deviceId);
    }
}

void KeyboardWidget::onOutputDeviceChanged(int index) {
    if (index >= 0) {
        QString deviceId = outputDeviceCombo->itemData(index).toString();
        emit midiOutputDeviceChanged(deviceId);
    }
}

void KeyboardWidget::refreshDevices() {
    if (deviceRefreshCallback) {
        deviceRefreshCallback();
    }
}

void KeyboardWidget::setupMidiCIControls() {
    midiCIGroup = new QGroupBox("MIDI-CI Status");
    midiCIGroup->setMaximumHeight(200); // Slightly taller for combobox
    QVBoxLayout* midiCILayout = new QVBoxLayout(midiCIGroup);
    midiCILayout->setSpacing(5); // Reduce spacing
    
    // Top row: Status and MUID in one line
    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Status:"));
    midiCIStatusLabel = new QLabel("Not Initialized");
    midiCIStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    topLayout->addWidget(midiCIStatusLabel);
    topLayout->addSpacing(20);
    topLayout->addWidget(new QLabel("MUID:"));
    midiCIMuidLabel = new QLabel("N/A");
    topLayout->addWidget(midiCIMuidLabel);
    topLayout->addStretch();
    midiCILayout->addLayout(topLayout);
    
    // Device name row
    QHBoxLayout* deviceNameLayout = new QHBoxLayout();
    deviceNameLayout->addWidget(new QLabel("Device:"));
    midiCIDeviceNameLabel = new QLabel("N/A");
    deviceNameLayout->addWidget(midiCIDeviceNameLabel);
    deviceNameLayout->addStretch();
    midiCILayout->addLayout(deviceNameLayout);
    
    // Discovery button
    QHBoxLayout* discoveryLayout = new QHBoxLayout();
    midiCIDiscoveryButton = new QPushButton("Send Discovery");
    midiCIDiscoveryButton->setEnabled(false);
    midiCIDiscoveryButton->setMaximumWidth(120);
    connect(midiCIDiscoveryButton, &QPushButton::clicked, this, &KeyboardWidget::sendMidiCIDiscovery);
    discoveryLayout->addWidget(midiCIDiscoveryButton);
    discoveryLayout->addStretch();
    midiCILayout->addLayout(discoveryLayout);
    
    // Device selection combobox
    QHBoxLayout* deviceSelectionLayout = new QHBoxLayout();
    deviceSelectionLayout->addWidget(new QLabel("Select Device:"));
    midiCIDeviceCombo = new QComboBox();
    midiCIDeviceCombo->setEnabled(false);
    midiCIDeviceCombo->addItem("No devices discovered");
    connect(midiCIDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &KeyboardWidget::onMidiCIDeviceSelected);
    deviceSelectionLayout->addWidget(midiCIDeviceCombo, 1);
    midiCILayout->addLayout(deviceSelectionLayout);
    
    // Selected device detailed info
    midiCISelectedDeviceInfo = new QLabel("Select a MIDI-CI device to view details");
    midiCISelectedDeviceInfo->setWordWrap(true);
    midiCISelectedDeviceInfo->setMaximumHeight(50);
    midiCISelectedDeviceInfo->setStyleSheet("font-size: 11px; background-color: #f5f5f5; padding: 4px; border: 1px solid #ccc;");
    midiCILayout->addWidget(midiCISelectedDeviceInfo);
    
    mainLayout->addWidget(midiCIGroup);
}

void KeyboardWidget::updateMidiCIStatus(bool initialized, uint32_t muid, const std::string& deviceName) {
    if (initialized) {
        midiCIStatusLabel->setText("Initialized");
        midiCIStatusLabel->setStyleSheet("color: green; font-weight: bold;");
        midiCIMuidLabel->setText(QString("0x%1 (%2)").arg(CIFactory::midiCI32to28(muid), 0, 16).arg(muid));
        midiCIDeviceNameLabel->setText(QString::fromStdString(deviceName));
        midiCIDiscoveryButton->setEnabled(true);
    } else {
        midiCIStatusLabel->setText("Not Initialized");
        midiCIStatusLabel->setStyleSheet("color: red; font-weight: bold;");
        midiCIMuidLabel->setText("N/A");
        midiCIDeviceNameLabel->setText("N/A");
        midiCIDiscoveryButton->setEnabled(false);
    }
}

void KeyboardWidget::updateMidiCIDevices(const std::vector<MidiCIDeviceInfo>& discoveredDevices) {
    // Clear and repopulate the combobox
    midiCIDeviceCombo->clear();
    
    if (discoveredDevices.empty()) {
        midiCIDeviceCombo->addItem("No devices discovered");
        midiCIDeviceCombo->setEnabled(false);
        midiCISelectedDeviceInfo->setText("No MIDI-CI devices discovered. Send discovery to find devices.");
    } else {
        midiCIDeviceCombo->setEnabled(true);
        for (const auto& device : discoveredDevices) {
            QString displayName = QString::fromStdString(device.getDisplayName());
            midiCIDeviceCombo->addItem(displayName, device.muid);
        }
        
        // Auto-select first device if available
        if (!discoveredDevices.empty()) {
            onMidiCIDeviceSelected(0);
        }
    }
}

void KeyboardWidget::setMidiCIDiscoveryCallback(std::function<void()> callback) {
    midiCIDiscoveryCallback = callback;
}

void KeyboardWidget::setMidiCIUpdateCallback(std::function<void()> callback) {
    midiCIUpdateCallback = callback;
}

void KeyboardWidget::setMidiCIDeviceProvider(std::function<MidiCIDeviceInfo*(uint32_t)> provider) {
    midiCIDeviceProvider = provider;
}

void KeyboardWidget::sendMidiCIDiscovery() {
    if (midiCIDiscoveryCallback) {
        midiCIDiscoveryCallback();
    }
}

void KeyboardWidget::updateMidiCIPeriodically() {
    if (midiCIUpdateCallback) {
        midiCIUpdateCallback();
    }
}

void KeyboardWidget::onMidiCIDeviceSelected(int index) {
    if (index < 0 || !midiCIDeviceProvider) {
        midiCISelectedDeviceInfo->setText("No device selected");
        return;
    }
    
    QVariant muidVariant = midiCIDeviceCombo->itemData(index);
    if (!muidVariant.isValid()) {
        midiCISelectedDeviceInfo->setText("Invalid device selection");
        return;
    }
    
    uint32_t muid = muidVariant.toUInt();
    MidiCIDeviceInfo* device = midiCIDeviceProvider(muid);
    
    if (device) {
        QString info = QString("MUID: 0x%1 (%2)\nManufacturer: %3\nModel: %4\nVersion: %5")
                       .arg(muid, 0, 16)
                       .arg(muid)
                       .arg(QString::fromStdString(device->manufacturer))
                       .arg(QString::fromStdString(device->model))
                       .arg(QString::fromStdString(device->version));
        midiCISelectedDeviceInfo->setText(info);
    } else {
        midiCISelectedDeviceInfo->setText("Device information not available");
    }
}

#include "keyboard_widget.moc"