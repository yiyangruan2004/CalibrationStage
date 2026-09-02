#pragma once
#include <QObject>
#include <QString>
#include <QSettings>

#ifndef Q_OS_WASM
#include "visa.h"
#endif
#include "device.h"


class Fg : public Device
{
    Q_OBJECT

public:
    explicit Fg(QObject *parent = nullptr);
    ~Fg();

    QString id;
    DeviceState connect(bool connection);

    DeviceState config();
    int wave;
    int amp;
    int freq;
    int cyc;

    DeviceState trig();
private:
#ifndef Q_OS_WASM
    ViSession rmSession;
    ViSession instrSession;
    DeviceState write(const char *cmd);
#endif
};


