#include "vmx.h"

#ifndef Q_OS_WASM
Coord operator-(const Coord& a, const Coord& b){
    return {
        a.X - b.X,
        a.Y - b.Y,
        a.Z - b.Z
    };
}
#endif

Vmx::Vmx(QObject *parent)
    : Device(parent)
    , killFlag(false)
{}

#ifndef Q_OS_WASM
QByteArray Vmx::read(const QByteArray &term){
    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        if (port.bytesAvailable() > 0) {
            response += port.readAll();
            if (response.contains(term)) {
                break;
            }
        }
        QThread::msleep(10);
    }
    // a kill that writes stuff may happen
    return response;
}

DeviceState Vmx::write(const QByteArray &cmd){
    port.clear();
    port.write(cmd);
    if (!port.waitForBytesWritten(1000)) {
        qWarning() << "Motor not responding.";
        deviceState = offline;
    }
    return deviceState;
}
#endif

DeviceState Vmx::connect(bool connection){
#ifdef Q_OS_WASM
    deviceState = connection ? ready : offline;
    if (connection) {
        emit updateCoord();
    }
    qDebug() << "vmx.simulation.connect:" << deviceState;
    return deviceState;
#else
    if (connection){
        port.setPortName(serial);
        port.setBaudRate(QSerialPort::Baud9600);
        port.setDataBits(QSerialPort::Data8);
        port.setParity(QSerialPort::NoParity);
        port.setStopBits(QSerialPort::OneStop);
        port.setFlowControl(QSerialPort::NoFlowControl);
        port.open(QIODevice::ReadWrite);
    }else{
        port.close();
    }

    if (port.isOpen()){
        write("F");
        write("K");
        write("V");
        QByteArray response = read("R");
        if (response == "R") {
            emit updateCoord();
            deviceState = ready;
        } else {
            qWarning() << "Restart motor control unit \n Unexpected motor response: " << response;
            deviceState = error;
        }
    } else {
        if (connection){
            qWarning() << "Check serial port in usage with device manager \n Failed to open serial port";
            deviceState = offline;
        }
    }
    qDebug() << "vmx.connect: " << deviceState;
    return deviceState;
#endif
}



Coord Vmx::move(const Coord& goal){
#ifdef Q_OS_WASM
    if (deviceState == offline) {
        qWarning() << "Attempt to move simulated motor while offline";
        return pos;
    }
    pos = goal;
    emit updateCoord();
    return pos;
#else
    if(deviceState == offline){
        qWarning() << "Attempt to move motor while offline";
        return pos;
    }
    Coord dPos = goal - pos;
    QString cmd = "C ";
    if(dPos.X != 0){
        cmd += QString("I%1M%2,").arg(1).arg(dPos.X);
    }
    if(dPos.Y != 0){
        cmd += QString("I%1M%2,").arg(2).arg(dPos.Y);
    }
    if(dPos.Z != 0){
        cmd += QString("I%1M%2,").arg(3).arg(dPos.Z);
    }
    cmd += "R";
    write(cmd.toUtf8());

    QByteArray response;
    response = read("^");
    pos = goal;
    return goal;
#endif
}


Coord Vmx::coord() {
#ifdef Q_OS_WASM
    if (deviceState == offline) {
        qWarning() << "Attempt to read simulated motor coordinates while offline";
        return pos;
    }
    emit updateCoord();
    return pos;
#else
    if(deviceState == offline){
        qWarning() << "Attempt to move motor while offline";
        return pos;
    }
    QByteArray response;
    write("X");
    response = read("\r");
    pos.X = response.trimmed().toInt();
    write("Y");
    response = read("\r");
    pos.Y = response.trimmed().toInt();
    write("Z");
    response = read("\r");
    pos.Z = response.trimmed().toInt();
    qDebug() << "vmx.coord: " << pos.X << "," << pos.Y << "," << pos.Z;
    emit updateCoord();
    return pos;
#endif
}

DeviceState Vmx::zero(){
#ifdef Q_OS_WASM
    if (deviceState == offline) {
        qWarning() << "Attempt to zero simulated motor while offline";
        return deviceState;
    }
    pos = {};
    emit updateCoord();
    return deviceState;
#else
    if(deviceState == offline){
        qWarning() << "Attempt to move motor while offline";
        return deviceState;
    }
    write("N");
    qDebug() << "vmx.zero";
    return deviceState;
#endif
}

DeviceState Vmx::kill(){
#ifdef Q_OS_WASM
    qDebug() << "vmx.simulation.kill";
    return deviceState;
#else
    if(deviceState == offline){
        qWarning() << "Attempt to move motor while offline";
        return deviceState;
    }
    write("K");
    qDebug() << "vmx.kill";
    return deviceState;
#endif
}

Vmx::~Vmx(){
    connect(false);
}
