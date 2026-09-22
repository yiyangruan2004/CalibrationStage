#include "fg.h"

Fg::Fg(QObject *parent) : Device(parent) {}

#ifndef Q_OS_WASM
DeviceState Fg::write(const char* cmd){
    viPrintf(instrSession, "%s\n", cmd);
    return deviceState;
}
#endif

DeviceState Fg::connect(bool connection){
#ifndef Q_OS_WASM
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
            connection = false;
        }
    }else{
        viClose(instrSession);
        instrSession = VI_NULL;
        viClose(rmSession);
        rmSession = VI_NULL;
        //destroy object when disconnect
    }
#endif
    deviceState = connection ? online : offline;
    if (connection) {
        config();
    }
    qDebug() << "fg.connect:" << deviceState;
    return deviceState;
}

DeviceState Fg::config(){
    if(deviceState == offline){
        qWarning() << "Attempt to configure function generator while offline";
        return deviceState;
    }
#ifndef Q_OS_WASM
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
#endif
    deviceState = ready;
    qDebug() << "fg.config: " << deviceState;
    return deviceState;
}

DeviceState Fg::trig(){
    if(deviceState != ready){
        qWarning() << "Attempt to trigger function generator while not configured";
        return deviceState;
    }
#ifndef Q_OS_WASM
    write("TRIG1");
#endif
    return deviceState;
}

Fg::~Fg(){
    connect(false);
}

