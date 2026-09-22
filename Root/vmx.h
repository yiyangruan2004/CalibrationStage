#ifndef VMX_H
#define VMX_H

#include <QDebug>
#include <QString>
#include <QThread>
#include <cmath>

#ifndef Q_OS_WASM
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSerialPort>
#endif

#include "device.h"

#define STEP_SIZE 0.00025
inline int inchToSteps(double inch)
{
    return static_cast<int>(std::lround(inch / STEP_SIZE));
}

struct Coord{
    int X=0;
    int Y=0;
    int Z=0;
};

class Vmx: public Device
{
    Q_OBJECT

public:
    explicit Vmx(QObject *parent = nullptr);
    ~Vmx();
    DeviceState connect(bool connection);
    QString serial;

    Coord move(const Coord& dPos);
    int steps;

    Coord coord();
    Coord pos;

    DeviceState zero();
    DeviceState kill();
    std::atomic_bool killflag{false};


signals:
    void killed();
    void updateCoord(Coord position);

private:
#ifndef Q_OS_WASM
    QSerialPort port{this};
    QByteArray read(const QByteArray &term);
    DeviceState write(const QByteArray &cmd);
#endif
};




#endif // VMX_H
