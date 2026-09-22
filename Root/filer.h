#pragma once

#include <QtLogging>
#include <QApplication>
#include <QMessageBox>
#include <QQueue>
#include <QPair>
#include <QTimer>

#ifndef Q_OS_WASM
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QDateTime>
#include <QMutex>
#include <cstdio>
#include "vmx.h"
#include "pico.h"
#endif

class Filer
{
public:
    static void init();

#ifndef Q_OS_WASM
    struct Metadata
    {
        int frequencyKHz;
        int amplitudeMillivoltsPeakToPeak;
        int rangeMillivoltsPeakToPeak;
        int sampleCount;
        Coord position;
        double sensitivity;
    };

    static bool saveTrigger(const Metadata &metadata, const Data &data);
    static bool saveScan(const Metadata &metadata,
                         const QVector<Coord> &points, const QVector<double> &peaks);
#endif

};
