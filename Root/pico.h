#pragma once

#include <QDebug>
#include <QPointF>
#include <QVector>
#include <cmath>

#ifndef Q_OS_WASM
#include <QElapsedTimer>
#include <QThread>
#include <cstdint>
#include <vector>
#include "ps5000aApi.h"
#else
#include <algorithm>
#endif

#include "device.h"

struct Data{
    double peak = 0.0; //mV
    QVector<QPointF> points;
};

constexpr int volt[9]{
    10,
    20,
    50,
    100,
    200,
    500,
    1000,
    2000,
    5000,
};


class Pico : public Device
{
    Q_OBJECT

public:
    explicit Pico(QObject *parent = nullptr);
    ~Pico();
    DeviceState connect(bool connection);

    DeviceState config();
    int range;
    int samp;
    int offset;
    int timebase;
    double sens;

    DeviceState runBlock();

    Data read();

private:
#ifdef Q_OS_WASM
    Data simulationData;
#else
    int16_t handle = 0;
    const char* picoStatusToString(PICO_INFO status);
    std::vector<int16_t> buffer;
#endif
};
