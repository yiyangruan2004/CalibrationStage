#include "fg.h"


Fg::Fg(QObject *parent):
#ifndef Q_OS_WASM
    rmSession(VI_NULL),
    instrSession(VI_NULL)
#else
    Device(parent)
#endif
{}

#ifndef Q_OS_WASM
DeviceState Fg::write(const char* cmd){
    viPrintf(instrSession, "%s\n", cmd);
    return deviceState;
}
#endif

DeviceState Fg::connect(bool connection){
#ifdef Q_OS_WASM
    deviceState = connection ? online : offline;
    if (connection) {
        config();
    }
    qDebug() << "fg.simulation.connect:" << deviceState;
    return deviceState;
#else
    if (connection){
        QByteArray idBytes = id.toLocal8Bit();
        viOpenDefaultRM(&rmSession);
        viOpen(rmSession, idBytes.constData(), VI_NULL, 5000, &instrSession);
        write("*RST");
        write("*CLS");
        write("*IDN?");
        char buffer[128];
        ViStatus status = viRead(instrSession, (ViBuf)buffer, sizeof(buffer), nullptr);
        if (status != VI_SUCCESS) {
            char msg[128];
            viStatusDesc(instrSession, status, msg);
            qWarning() << "Restart function generator \n Check USB ID \n Failed to connect" << msg;
            deviceState = offline;
        } else {
            deviceState = online;
            config();
        }
    }else{
        viClose(instrSession);
        instrSession = VI_NULL;
        viClose(rmSession);
        rmSession = VI_NULL;
        deviceState = offline;
    }
    qDebug() << "fg.online: " << online;
    return deviceState;
#endif
}

DeviceState Fg::config(){
#ifdef Q_OS_WASM
    if (deviceState == offline) {
        qWarning() << "Attempt to configure simulated function generator while offline";
        return deviceState;
    }
    deviceState = ready;
    qDebug() << "fg.simulation.config:" << deviceState;
    return deviceState;
#else
    if(deviceState == offline){
        qWarning() << "Attempt to configure function generator while offline";
        return deviceState;
    }else{
        write("OUTPUT1 OFF");
        write(QString("SOUR1:FREQ %1").arg(freq*1000).toStdString().c_str());
        write(QString("SOUR1:VOLT %1").arg(amp/1000.0).toStdString().c_str());
        write("SOUR1:VOLT:OFFSET 0");
        write("SOUR1:FUNC SIN");
        write("SOUR1:BURS:STAT ON");
        write(QString("SOUR1:BURS:NCYC %1").arg(cyc).toStdString().c_str());
        write("SOUR1:BURS:INT:PER 1");
        write("OUTPUT:SYNC ON");
        write("OUTPUT:SYNC:SOURCE CH1");
        write("TRIG1:SOUR BUS");
        write("OUTPUT1 ON");
        deviceState = ready;
    }
    qDebug() << "fg.config: " << deviceState;
    return deviceState;
#endif
}

DeviceState Fg::trig(){
#ifdef Q_OS_WASM
    if (deviceState != ready) {
        qWarning() << "Attempt to trigger simulated function generator while not configured";
    }
    return deviceState;
#else
    if(deviceState != ready){
        qWarning() << "Attempt to trigger function generator while not configured";
        return deviceState;
    }
    write("TRIG1");
    return deviceState;
#endif
}

Fg::~Fg(){
    connect(false);
}

