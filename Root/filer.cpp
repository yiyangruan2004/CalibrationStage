#include "filer.h"

namespace {
QQueue<QPair<QtMsgType, QString>> pendingBoxes;
bool isShowingBox = false;

void showBox()
{
    if (isShowingBox || pendingBoxes.isEmpty()) {
        return;
    }

    const auto next = pendingBoxes.dequeue();
    const bool warning = next.first == QtWarningMsg;
    isShowingBox = true;

    auto *box = new QMessageBox(warning ? QMessageBox::Warning : QMessageBox::Information,
                               warning ? "Warning" : "Info", next.second);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QMessageBox::finished, box, [](int) {
        isShowingBox = false;
        QTimer::singleShot(0, QApplication::instance(), showBox);
    });
    box->show();
}

#ifndef Q_OS_WASM
bool openCsv(QFile &file, const QString &baseName, const Filer::Metadata &metadata)
{
    const QDir directory = QDir::current().filePath("../Data");
    if (!directory.exists() && !directory.mkpath(".")) {
        qWarning() << "Failed to create data directory";
        return false;
    }
    QString path = directory.filePath(baseName + ".csv");
    for (int suffix = 1; QFile::exists(path); ++suffix) {
        path = directory.filePath(QString("%1_%2.csv").arg(baseName).arg(suffix));
    }
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open data file:" << path;
        return false;
    }
    QTextStream stream(&file);
    stream.setRealNumberPrecision(12);
    stream << "Freq(kHz)," << metadata.frequencyKHz << '\n';
    stream << "Amp(mVpp)," << metadata.amplitudeMillivoltsPeakToPeak << '\n';
    stream << "Range(mVpp)," << metadata.rangeMillivoltsPeakToPeak << '\n';
    stream << "SampleCount," << metadata.sampleCount << '\n';
    stream << "X(in)," << metadata.position.X * STEP_SIZE << '\n';
    stream << "Y(in)," << metadata.position.Y * STEP_SIZE << '\n';
    stream << "Z(in)," << metadata.position.Z * STEP_SIZE << '\n';
    return true;
}
#endif
}

void Filer::init()
{
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
#ifndef Q_OS_WASM
        {
            static QMutex mutex;
            QMutexLocker locker(&mutex);
            const char *level = "UNKNOWN";
            switch (type) {
            case QtDebugMsg:    level = "DEBUG"; break;
            case QtInfoMsg:     level = "INFO"; break;
            case QtWarningMsg:  level = "WARNING"; break;
            case QtCriticalMsg: level = "CRITICAL"; break;
            case QtFatalMsg:    level = "FATAL"; break;
            }
            const QString formatted = QString("[%1] [%2] %3")
                                          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
                                          .arg(level)
                                          .arg(message);
            fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
            static FILE *file = fopen("log.txt", "a");
            if (file != nullptr) {
                fprintf(file, "%s\n", formatted.toUtf8().constData());
                fflush(file);
            }
        }
#endif
        if ((type == QtInfoMsg || type == QtWarningMsg) && QApplication::instance() != nullptr) {
            QMetaObject::invokeMethod(QApplication::instance(), [type, message] {
                pendingBoxes.enqueue({type, message});
                showBox();
            }, Qt::QueuedConnection);
        }
    });
#ifndef Q_OS_WASM
    qDebug() << "Logger installed";
#endif
}

#ifndef Q_OS_WASM
bool Filer::saveTrigger(const Metadata &metadata, const Data &data)
{
    QFile file;
    if (!openCsv(file, "Trig", metadata)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "P(kPa)," << data.peak / metadata.sensitivity << "\n";
    stream << "\n Voltage(mV) \n";
    for (const QPointF &point : data.points) {
        stream << point.y() << '\n';
    }
    return true;
}

bool Filer::saveScan(const Metadata &metadata,
                     const QVector<Coord> &points, const QVector<double> &peaks)
{
    if (points.size() != peaks.size()) {
        qWarning() << "Scan points and peaks do not match";
        return false;
    }
    QFile file;
    if (!openCsv(file, "Scan", metadata)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setRealNumberPrecision(12);
    stream << "\nX(in),Y(in),Z(in),P(kPa)\n";
    for (int index = 0; index < points.size(); ++index) {
        const Coord &point = points.at(index);
        stream << point.X * STEP_SIZE << ',' << point.Y * STEP_SIZE << ',' << point.Z * STEP_SIZE
               << ',' << peaks.at(index) / metadata.sensitivity << '\n';
    }
    return true;
}
#endif
