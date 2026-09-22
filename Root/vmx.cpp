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

Vmx::Vmx(QObject *parent) : Device(parent) {}

#ifndef Q_OS_WASM
QByteArray Vmx::read(const QByteArray &term){
    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        if (term == "^" && killflag) break;
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
        emit updateCoord(pos);
    }
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
            emit updateCoord(pos);
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
#endif
    qDebug() << "vmx.connect: " << deviceState;
    return deviceState;
}



Coord Vmx::move(const Coord& goal){
    if(deviceState == offline){
        qWarning() << "Attempt to move motor while offline";
        return pos;
    }
#ifndef Q_OS_WASM
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

    read("^");
#else
    QThread::msleep(100);
#endif
    if (killflag) return pos;
    pos = goal;
#ifdef Q_OS_WASM
    emit updateCoord(pos);
#endif
    return pos;
}


Coord Vmx::coord() {
    if(deviceState == offline){
        qWarning() << "Attempt to read motor coordinates while offline";
        return pos;
    }
#ifndef Q_OS_WASM
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
#endif
    emit updateCoord(pos);
    return pos;
}

DeviceState Vmx::zero(){
    if(deviceState == offline){
        qWarning() << "Attempt to zero motor while offline";
        return deviceState;
    }
#ifdef Q_OS_WASM
    pos = {};
    emit updateCoord(pos);
#else
    write("N");
#endif
    qDebug() << "vmx.zero";
    return deviceState;
}

DeviceState Vmx::kill(){
    killflag = true;
    emit killed();
#ifndef Q_OS_WASM
    // During a scan, the worker sends the command after observing killflag.
    if (QThread::currentThread() == thread()) {
        if (deviceState == offline) {
            qWarning() << "Attempt to move motor while offline";
            return deviceState;
        }
        write("K");
    }
#endif
    qDebug() << "vmx.kill";
    return deviceState;
}

Vmx::~Vmx(){
    connect(false);
}
