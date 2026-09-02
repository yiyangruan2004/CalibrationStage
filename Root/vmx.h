#ifndef VMX_H
#define VMX_H

#include <QSettings>
#include <QObject>
#include <QString>
#ifndef Q_OS_WASM
#include <QSerialPort>
#include <QSerialPortInfo>
#endif
#include <QElapsedTimer>
#include <QApplication>
#include <QThread>
#include <cmath>
#include "device.h"

#define STEP_SIZE 0.00025
#define SENS 2.149e-001 //(mV/kPa)
inline int coordinateToSteps(double coordinate)
{
    return static_cast<int>(std::lround(coordinate / STEP_SIZE));
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
    bool killFlag;


signals:
    void updateCoord();

private:
#ifndef Q_OS_WASM
    QSerialPort port;
    QByteArray read(const QByteArray &term);
    DeviceState write(const QByteArray &cmd);
#endif
};




#endif // VMX_H
