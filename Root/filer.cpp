#include "filer.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QMessageBox>
#include <QQueue>
#include <QTextStream>
#include <QTimer>

QMutex Filer::mutex;
QtMessageHandler Filer::originalHandler = nullptr;

namespace {
struct PendingBox {
    QMessageBox::Icon icon;
    QString title;
    QString text;
};

QQueue<PendingBox> pendingBoxes;
bool isShowingBox = false;

void showNextBox()
{
    if (isShowingBox || pendingBoxes.isEmpty()) {
        return;
    }

    const PendingBox next = pendingBoxes.dequeue();
    isShowingBox = true;

    auto *box = new QMessageBox(next.icon, next.title, next.text);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QMessageBox::finished, box, [](int) {
        isShowingBox = false;
        QTimer::singleShot(0, QApplication::instance(), [] {
            showNextBox();
        });
    });
    box->show();
}

void queueMessageBox(QtMsgType type, const QString &message)
{
    switch (type) {
    case QtInfoMsg:
        pendingBoxes.enqueue({QMessageBox::Information, "Info", message});
        break;
    case QtWarningMsg:
        pendingBoxes.enqueue({QMessageBox::Warning, "Warning", message});
        break;
    default:
        return;
    }
    showNextBox();
}
}

QString Filer::levelToString(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO";
    case QtWarningMsg:  return "WARNING";
    case QtCriticalMsg: return "CRITICAL";
    case QtFatalMsg:    return "FATAL";
    }
    return "UNKNOWN";
}

void Filer::logToFile(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    QMutexLocker locker(&mutex);
    const QString formatted = QString("[%1] [%2] %3")
                                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                                  .arg(levelToString(type))
                                  .arg(message);

    fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());

    if ((type == QtInfoMsg || type == QtWarningMsg) && QApplication::instance() != nullptr) {
        QMetaObject::invokeMethod(QApplication::instance(), [type, message] {
            queueMessageBox(type, message);
        }, Qt::QueuedConnection);
    }

    static FILE *file = fopen("log.txt", "a");
    if (file != nullptr) {
        fprintf(file, "%s\n", formatted.toUtf8().constData());
        fflush(file);
    }
}

void Filer::installLogger()
{
    originalHandler = qInstallMessageHandler(Filer::logToFile);
    qDebug() << "Logger installed";
}

QString Filer::nextAvailableCsvPath(const QDir &directory, const QString &baseName)
{
    QString filePath = directory.filePath(baseName + ".csv");
    for (int suffix = 1; QFile::exists(filePath); ++suffix) {
        filePath = directory.filePath(QString("%1_%2.csv").arg(baseName).arg(suffix));
    }
    return filePath;
}

void Filer::writeCaptureMetadata(QTextStream &stream, const CaptureMetadata &metadata)
{
    stream << "Freq(kHz)," << metadata.frequencyKHz << '\n';
    stream << "Amp(mVpp)," << metadata.amplitudeMillivoltsPeakToPeak << '\n';
    stream << "Range(mVpp)," << metadata.rangeMillivoltsPeakToPeak << '\n';
    stream << "SampleCount," << metadata.sampleCount << '\n';
    stream << "X," << metadata.position.X << '\n';
    stream << "Y," << metadata.position.Y << '\n';
    stream << "Z," << metadata.position.Z << '\n';
}

bool Filer::saveTrigger(const QDir &directory, const CaptureMetadata &metadata,
                        double peakMillivolts, const QVector<QPointF> &points)
{
    if (!directory.exists() && !directory.mkpath(".")) {
        qWarning() << "Failed to create trigger data directory";
        return false;
    }

    QFile file(nextAvailableCsvPath(directory, "Trig"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open trigger data file";
        return false;
    }

    QTextStream stream(&file);
    writeCaptureMetadata(stream, metadata);
    stream << "P(kPa)," << peakMillivolts / SENS << "\n";
    stream << "\n Points \n";
    for (const QPointF &point : points) {
        stream << point.y() << '\n';
    }
    return true;
}

bool Filer::saveScan(const QDir &directory, const CaptureMetadata &metadata,
                     const QVector<Coord> &points, const QVector<double> &peaks)
{
    if (points.size() != peaks.size()) {
        qWarning() << "Scan points and peaks do not match";
        return false;
    }
    if (!directory.exists() && !directory.mkpath(".")) {
        qWarning() << "Failed to create scan data directory";
        return false;
    }

    QFile file(nextAvailableCsvPath(directory, "Scan"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open scan data file";
        return false;
    }

    QTextStream stream(&file);
    writeCaptureMetadata(stream, metadata);
    stream << "\n Pressures(kPa) \n";
    for (int index = 0; index < points.size(); ++index) {
        const Coord &point = points.at(index);
        stream << point.X << ',' << point.Y << ',' << point.Z << ',' << peaks.at(index) / SENS << '\n';
    }
    return true;
}
