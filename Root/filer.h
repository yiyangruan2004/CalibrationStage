#pragma once

#include <QDir>
#include <QMutex>
#include <QPointF>
#include <QtLogging>
#include <QVector>

#include "vmx.h"

class QTextStream;

class Filer
{
public:
    struct CaptureMetadata
    {
        int frequencyKHz;
        int amplitudeMillivoltsPeakToPeak;
        int rangeMillivoltsPeakToPeak;
        int sampleCount;
        Coord position;
    };

    static void installLogger();
    static QString nextAvailableCsvPath(const QDir &directory, const QString &baseName);
    static void writeCaptureMetadata(QTextStream &stream, const CaptureMetadata &metadata);
    static bool saveTrigger(const QDir &directory, const CaptureMetadata &metadata,
                            double peakMillivolts, const QVector<QPointF> &points);
    static bool saveScan(const QDir &directory, const CaptureMetadata &metadata,
                         const QVector<Coord> &points, const QVector<double> &peaks);

private:
    static QString levelToString(QtMsgType type);
    static void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &message);

    static QMutex mutex;
    static QtMessageHandler originalHandler;
};
