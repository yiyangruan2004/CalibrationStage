#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <QtCharts/QLineSeries>
#include <vector>
#include <QElapsedTimer>
#include <QThread>
#include <random>
#include <cmath>
#ifndef Q_OS_WASM
#include "ps5000aApi.h"
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

    DeviceState runBlock();

    Data read();
    void setSimulationGain(double gain);
    void setSimulationTimeShift(double timeShift);

private:
#ifndef Q_OS_WASM
    int16_t handle;
    const char* picoStatusToString(PICO_INFO status);
    std::vector<int16_t> buffer;
#else
    double simulationGain = 1.0;
    double simulationTimeShift = 0.0;
    std::mt19937 simulationGenerator{0xC411B};
#endif
};
