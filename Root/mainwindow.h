#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QMargins>
#include <QSettings>
#include <QMetaObject>
#include <QTimer>
#include <QSignalBlocker>
#include <QCloseEvent>

#include "pico.h"
#include "vmx.h"
#include "fg.h"
#include "scan.h"
#include "filer.h"
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
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    friend class ScanLifecycleTests;
    Ui::MainWindow *ui;
    QSettings settings;
    Pico pico;
    Fg fg;
    Vmx vmx;

    Scan scan{pico, fg, vmx};


    QLineSeries *series = new QLineSeries();
    QChart *chart = new QChart();
    QValueAxis *axisX = new QValueAxis();
    QValueAxis *axisY = new QValueAxis();
    Coord minCorner;
    Coord maxCorner;
    void applyDashboardTheme();
    void checkConfig(Device *device);

    // Binding
    void bindConnection(QPushButton *btn, QCheckBox *stat, Device *device);
    void bindLine(QLineEdit *line, const QString &key, Device *device, QString &str);
    void bindSpinBox(QSpinBox *spinBox, const QString &key, Device *device, int &num);
    void bindDoubleBox(QDoubleSpinBox *spinBox, const QString &key, Device *device, double &num);
    void bindCoordBox(QDoubleSpinBox *coordBox, const QString &key, Device *device, int &num);
    void bindComboBox(QComboBox *comboBox, const QString &key, Device *device, int &idx);
    void bindMove(QPushButton *btn, int dx, int dy, int dz);
};


#endif // MAINWINDOW_H
