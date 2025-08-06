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

signals:
    void midiInputDeviceChanged(const QString& deviceId);
    void midiOutputDeviceChanged(const QString& deviceId);

private slots:
    void onKeyPressed(int note);
    void onKeyReleased(int note);
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void refreshDevices();

private:
    void setupUI();
    void setupKeyboard();
    void setupDeviceSelectors();
    QWidget* createKeyboardWidget();
    
    std::function<void(int)> keyPressedCallback;
    std::function<void(int)> keyReleasedCallback;
    std::function<void()> deviceRefreshCallback;
    
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
    
    QList<PianoKey*> whiteKeys;
    QList<PianoKey*> blackKeys;
    
    QSignalMapper* pressMapper;
    QSignalMapper* releaseMapper;
};