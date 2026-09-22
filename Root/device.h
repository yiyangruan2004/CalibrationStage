#pragma once
#include <QObject>
#include <atomic>

enum DeviceState {
    offline,
    online,
    ready,
    error
};

class Device : public QObject
{
    Q_OBJECT

public:
    explicit Device(QObject *parent = nullptr);
    ~Device() override = default;
    virtual DeviceState connect(bool connection) = 0;
    std::atomic<DeviceState> deviceState;
};
