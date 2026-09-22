#pragma once

#include <QDebug>
#include <QVector>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <limits>

#include "vmx.h"
#include "filer.h"
#include "fg.h"
#include "pico.h"

enum scanState {
    Idle, Checking, Ready, Scanning, Paused, Killed, Error
};

class Scan : public QThread
{
    Q_OBJECT
public:
    Scan(Pico &pico, Fg &fg, Vmx &vmx);
    ~Scan() override;
    std::atomic<scanState> state{Idle};
    void checkBound(const Coord &minimum, const Coord &maximum);
    void resume();
    void pause();

signals:
    void stateChanged(scanState state);
    void measured(Data data, int completed, int total);

private:
    void run() override;
    QMutex mutex;
    QWaitCondition wake;
    Coord minimum, maximum;
    Pico &pico;
    Fg &fg;
    Vmx &vmx;
    QVector<Coord> points;
    QVector<double> peaks;
};
