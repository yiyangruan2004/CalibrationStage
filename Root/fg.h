#pragma once
#include <QDebug>
#include <QString>

#ifndef Q_OS_WASM
#include <QByteArray>
#include <string>
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
    ViSession rmSession = VI_NULL;
    ViSession instrSession = VI_NULL;
    DeviceState write(const char *cmd);
#endif
};


