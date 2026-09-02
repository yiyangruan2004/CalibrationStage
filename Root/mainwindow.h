#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <memory>

#include "pico.h"
#include "vmx.h"
#include "fg.h"
#include "scan.h"
#include "./ui_mainwindow.h"

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QDebug>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    QSettings settings;
    Pico pico;
    Fg fg;
    Vmx vmx;

    const QDir folderPath = QDir::current().filePath("../Data");
    std::unique_ptr<Scan> scan;
    QTimer scanTimer;
    ScanControlState scanState = ScanControlState::Idle;


    QLineSeries *series = new QLineSeries();
    QChart *chart = new QChart();
    QValueAxis *axisX = new QValueAxis();
    QValueAxis *axisY = new QValueAxis();
    Coord minCorner;
    Coord maxCorner;
    void applyDashboardTheme();
    void markDeviceConfigurationChanged(Device *device);
    void handleScanClick();
    void performBoundaryCheck();
    void advanceScan();
    void setScanState(ScanControlState state);
    void finishScan(Scan::Outcome outcome);
    void setScanControlsEnabled(bool enabled);
    void synchronizeScanConfiguration();

    // Binding
    void bindConnection(QPushButton *btn, QCheckBox *stat, Device *device);
    void bindLine(QLineEdit *line, const QString &key, Device *device, QString &str);
    void bindSpinBox(QSpinBox *spinBox, const QString &key, Device *device, int &num);
    void bindCoordBox(QDoubleSpinBox *coordBox, const QString &key, Device *device, int &num);
    void bindComboBox(QComboBox *comboBox, const QString &key, Device *device, int &idx);
    void bindMove(QPushButton *btn, int dx, int dy, int dz);
};


#endif // MAINWINDOW_H
