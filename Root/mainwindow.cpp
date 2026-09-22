#include "mainwindow.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    settings("MyCompany", "MyApp")
{
    ui->setupUi(this);
    applyDashboardTheme();

    // Connection ----------------------------------------------------------------------------------------------------------
    bindConnection(ui->picoConnect, ui->picoStat, &pico);
    bindConnection(ui->fgConnect, ui->fgStat, &fg);
    bindConnection(ui->vmxConnect, ui->vmxStat, &vmx);
    bindLine(ui->fgID, "fgID", &fg, fg.id);
    bindLine(ui->vmxSerial, "vmxSerial", &vmx, vmx.serial);

    // Operation ----------------------------------------------------------------------------------------------------------
    //fg config
    bindSpinBox(ui->fgAmp, "fgAmp", &fg, fg.amp);
    bindSpinBox(ui->fgFreq, "fgFreq", &fg, fg.freq);
    bindSpinBox(ui->fgCyc, "fgCyc", &fg, fg.cyc);
    connect(ui->fgConfig, &QPushButton::clicked, &fg, &Fg::config);
    //Vmx movement
    bindCoordBox(ui->vmxStep, "vmxStep", nullptr, vmx.steps);
    bindMove(ui->moveFront, -1,  0,  0);
    bindMove(ui->moveBack,   1,  0,  0);
    bindMove(ui->moveRight,  0, -1,  0);
    bindMove(ui->moveLeft,   0,  1,  0);
    bindMove(ui->moveUp,     0,  0, -1);
    bindMove(ui->moveDown,   0,  0,  1);
    bindCoordBox(ui->posX, "posX", nullptr, vmx.pos.X);
    bindCoordBox(ui->posY, "posY", nullptr, vmx.pos.Y);
    bindCoordBox(ui->posZ, "posZ", nullptr, vmx.pos.Z);

    connect(&vmx, &Vmx::updateCoord, this, [this](Coord position) {
        const QSignalBlocker x(ui->posX), y(ui->posY), z(ui->posZ);
        ui->posX->setValue(position.X*STEP_SIZE);
        ui->posY->setValue(position.Y*STEP_SIZE);
        ui->posZ->setValue(position.Z*STEP_SIZE);
    });
    connect(ui->vmxMove, &QPushButton::clicked, this, [this]() {
        ui->scan->setEnabled(false);
        Coord goal;
        goal.X = ui->posX->value()/STEP_SIZE;
        goal.Y = ui->posY->value()/STEP_SIZE;
        goal.Z = ui->posZ->value()/STEP_SIZE;
        vmx.coord();
        vmx.killflag = false;
        vmx.move(goal);
        vmx.coord();
        ui->scan->setEnabled(true);
    });
    connect(ui->vmxZero, &QPushButton::clicked, this, [this]() {
        ui->scan->setEnabled(false);
        vmx.zero();
        vmx.coord();
        ui->scan->setEnabled(true);
    });

    //Pico config
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    axisX->setTitleText("Sample index");
    axisY->setTitleText("Amplitude (mV)");
    chart->legend()->hide();
    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);
    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->picoRaw->setLayout(new QVBoxLayout());
    ui->picoRaw->layout()->addWidget(chartView);

    bindSpinBox(ui->picoSamp, "picoSamp", &pico, pico.samp);
    bindSpinBox(ui->picoOffset, "picoOffset", &pico, pico.offset);
    bindDoubleBox(ui->picoSens, "picoSens", &pico, pico.sens);
    bindComboBox(ui->picoRange, "picoRange", &pico, pico.range);

    bindSpinBox(ui->picoTimebase, "picoTimebase", &pico, pico.timebase);
    ui->picoFreq->setText(QString::number(250000 / pico.timebase));
    connect(ui->picoTimebase, QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int val){
        ui->picoFreq->setText(QString::number(250000 / val));
    });

    axisX->setRange(pico.offset, pico.samp);
    axisY->setRange(-volt[pico.range]/2, volt[pico.range]/2);
    connect(ui->picoConfig, &QPushButton::clicked, this, [this](){
        axisX->setRange(pico.offset, pico.samp);
        axisY->setRange(-volt[pico.range]/2, volt[pico.range]/2);
        pico.config();
    });


    connect(ui->fgTrig, &QPushButton::clicked, this, [this]() {
        if(pico.deviceState != ready){
            qWarning() << "Configure picoscope first";
            return;
        }
        if(fg.deviceState != ready){
            qWarning() << "Configure function generator first";
            return;
        }
        ui->fgTrig->setEnabled(false);
        ui->scan->setEnabled(false);
        QApplication::processEvents();
        pico.runBlock();
        fg.trig();
        const Data data = pico.read();

        series->clear();
        ui->pressure->setText(QString::number(data.peak / pico.sens));
        series->replace(data.points);
#ifndef Q_OS_WASM
        Filer::saveTrigger({fg.freq, fg.amp, volt[pico.range], pico.samp, vmx.pos, pico.sens}, data);
#endif
        ui->fgTrig->setEnabled(true);
        ui->scan->setEnabled(true);
    });

    //Scan
    bindCoordBox(ui->minX, "minX", nullptr, minCorner.X);
    bindCoordBox(ui->minY, "minY", nullptr, minCorner.Y);
    bindCoordBox(ui->minZ, "minZ", nullptr, minCorner.Z);
    bindCoordBox(ui->maxX, "maxX", nullptr, maxCorner.X);
    bindCoordBox(ui->maxY, "maxY", nullptr, maxCorner.Y);
    bindCoordBox(ui->maxZ, "maxZ", nullptr, maxCorner.Z);
    const auto clearScanDisplays = [this]() {
        ui->pressure->clear();
        ui->pointCNT->reset();
        ui->pointCNT->setFormat("");
    };
    connect(&scan, &Scan::stateChanged, this, [this, clearScanDisplays](scanState state) {
        if (state != scan.state) return;
        const bool running = scan.isRunning();
        for (QWidget *widget : findChildren<QWidget *>()) {
            if (qobject_cast<QPushButton *>(widget)) {
                // Keep a pressed scan button enabled while progress changes.
                widget->setEnabled(!running || (widget == ui->vmxKill && state != Killed)
                    || (widget == ui->scan && (state == Ready || state == Scanning)));
            } else if (qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QLineEdit *>(widget)
                       || qobject_cast<QComboBox *>(widget)) {
                widget->setEnabled(!running);
            }
        }
        switch (state) {
        case Idle:
        case Error:
            ui->scan->setText("Scan");
            if (state == Error) ui->scanStat->setText("Scan unavailable");
            else if (!running) {
                clearScanDisplays();
                ui->scanStat->setText("Scan complete");
                qInfo() << "Scan complete";
            }
            break;
        case Checking:
            ui->scan->setText("N/A");
            ui->scanStat->setText("Checking");
            break;
        case Ready:
            ui->scan->setText("Resume");
            ui->scanStat->setText("Ready");
            break;
        case Scanning:
            ui->scan->setText("Pause");
            ui->scanStat->setText("Scanning");
            break;
        case Paused:
            ui->scan->setText(running ? "N/A" : "Continue");
            ui->scanStat->setText(running ? "Pausing" : "Paused");
            break;
        case Killed:
            clearScanDisplays();
            ui->scan->setText(running ? "N/A" : "Scan");
            ui->scanStat->setText(running ? "Killed" : "Scan cancelled");
            break;
        }
    });
    connect(&scan, &Scan::measured, this, [this](Data data, int completed, int total) {
        const scanState state = scan.state.load();
        if (vmx.killflag || (state != Scanning && state != Paused)) return;
        ui->pointCNT->setRange(0, total);
        ui->pointCNT->setValue(completed);
        ui->pointCNT->setFormat(QString("%1 remaining").arg(total - completed));
        if (!data.points.isEmpty()) {
            series->replace(data.points);
            ui->pressure->setText(QString::number(data.peak / pico.sens));
        }
    });
    connect(ui->scan, &QPushButton::clicked, this, [this]() {
        if (pico.deviceState != ready || fg.deviceState != ready || vmx.deviceState != ready) {
            qWarning() << "Configure all devices before scanning";
            return;
        }
        switch (scan.state.load()) {
        case Idle:
        case Error:
        case Killed:
            vmx.steps = inchToSteps(ui->vmxStep->value());
            minCorner = {inchToSteps(ui->minX->value()), inchToSteps(ui->minY->value()), inchToSteps(ui->minZ->value())};
            maxCorner = {inchToSteps(ui->maxX->value()), inchToSteps(ui->maxY->value()), inchToSteps(ui->maxZ->value())};
            scan.checkBound(minCorner, maxCorner);
            break;
        case Ready:
        case Paused:
            scan.resume();
            break;
        case Scanning:
            scan.pause();
            break;
        case Checking:
            return;
        }
    });
    connect(ui->vmxKill, &QPushButton::clicked, this, [this, clearScanDisplays]() {
        clearScanDisplays();
        if (scan.isRunning()) {
            vmx.kill();
            return;
        }
        ui->scan->setEnabled(false);
        vmx.kill();
        // A paused scan's cancellation signal refreshes the controls.
        ui->scan->setEnabled(false);
        vmx.coord();
        ui->scan->setEnabled(true);
    });
}

void MainWindow::applyDashboardTheme(){
    ui->fgTrig->setProperty("buttonRole", "primary");
    ui->scan->setProperty("buttonRole", "primary");
    ui->vmxKill->setProperty("buttonRole", "danger");

    ui->picoConnect->setProperty("buttonSize", "compact");
    ui->fgConnect->setProperty("buttonSize", "compact");
    ui->vmxConnect->setProperty("buttonSize", "compact");

    ui->moveFront->setProperty("buttonRole", "move");
    ui->moveBack->setProperty("buttonRole", "move");
    ui->moveLeft->setProperty("buttonRole", "move");
    ui->moveRight->setProperty("buttonRole", "move");
    ui->moveUp->setProperty("buttonRole", "move");
    ui->moveDown->setProperty("buttonRole", "move");

    ui->moveFront->setToolTip("Move X negative (W)");
    ui->moveBack->setToolTip("Move X positive (S)");
    ui->moveLeft->setToolTip("Move Y positive (A)");
    ui->moveRight->setToolTip("Move Y negative (D)");
    ui->moveUp->setToolTip("Move Z negative (Up)");
    ui->moveDown->setToolTip("Move Z positive (Down)");

    const QString style = R"qss(
        QMainWindow {
            background-color: #f4f7fb;
        }

        QWidget#centralwidget {
            background-color: #f4f7fb;
            color: #1f2937;
            font-family: "Segoe UI";
            font-size: 9pt;
        }

        QTabWidget::pane {
            background-color: #ffffff;
            border: 1px solid #cfd8e3;
            border-radius: 8px;
            top: -1px;
        }

        QTabBar::tab {
            background-color: #e8eef7;
            border: 1px solid #cfd8e3;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            color: #374151;
            font-weight: 600;
            min-width: 96px;
            padding: 7px 16px;
        }

        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #2563eb;
        }

        QTabBar::tab:hover:!selected {
            background-color: #f0f5ff;
        }

        QGroupBox {
            background-color: #ffffff;
            border: 1px solid #cfd8e3;
            border-radius: 8px;
            color: #1f2937;
            font-weight: 600;
            margin-top: 14px;
            padding: 12px 8px 8px 8px;
        }

        QGroupBox::title {
            background-color: #ffffff;
            color: #2563eb;
            left: 10px;
            padding: 0 4px;
            subcontrol-origin: margin;
        }

        QLabel {
            color: #374151;
            font-weight: 500;
        }

        QLabel#pressure,
        QLabel#label_11 {
            color: #1f2937;
            font-weight: 700;
        }

        QLineEdit,
        QComboBox {
            background-color: #ffffff;
            border: 1px solid #cfd8e3;
            border-radius: 6px;
            color: #1f2937;
            min-height: 22px;
            padding: 4px 8px;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QSpinBox,
        QDoubleSpinBox {
            background-color: #ffffff;
            border: 1px solid #cfd8e3;
            border-radius: 6px;
            color: #1f2937;
            min-height: 22px;
            padding: 4px 24px 4px 8px;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QLineEdit:focus,
        QComboBox:focus {
            border-color: #2563eb;
        }

        QSpinBox:focus,
        QDoubleSpinBox:focus {
            border-color: #2563eb;
        }

        QLineEdit:disabled,
        QComboBox:disabled {
            background-color: #eef2f7;
            color: #9ca3af;
        }

        QSpinBox:disabled,
        QDoubleSpinBox:disabled {
            background-color: #eef2f7;
            color: #9ca3af;
        }

        QComboBox::drop-down {
            border: none;
            width: 22px;
        }

        QComboBox QLineEdit {
            background-color: transparent;
            border: none;
            color: #1f2937;
            padding: 0;
        }

        QComboBox QAbstractItemView {
            background-color: #ffffff;
            border: 1px solid #cfd8e3;
            color: #1f2937;
            outline: 0;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QComboBox QAbstractItemView::item {
            color: #1f2937;
            min-height: 22px;
            padding: 4px 8px;
        }

        QComboBox QAbstractItemView::item:selected {
            background-color: #2563eb;
            color: #ffffff;
        }

        QPushButton {
            background-color: #eef4ff;
            border: 1px solid #b9c6d8;
            border-radius: 6px;
            color: #1f2937;
            font-weight: 600;
            min-height: 24px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background-color: #e0ebff;
            border-color: #2563eb;
        }

        QPushButton:pressed {
            background-color: #cfe0ff;
        }

        QPushButton:disabled {
            background-color: #e5e7eb;
            border-color: #d1d5db;
            color: #9ca3af;
        }

        QPushButton[buttonRole="primary"] {
            background-color: #15803d;
            border-color: #166534;
            color: #ffffff;
        }

        QPushButton[buttonRole="primary"]:hover {
            background-color: #166534;
            border-color: #14532d;
        }

        QPushButton[buttonRole="primary"]:pressed {
            background-color: #14532d;
        }

        QPushButton[buttonRole="move"] {
            background-color: #ffffff;
            border: 1px solid #94a3b8;
            border-radius: 8px;
            color: #2563eb;
            font-family: "Segoe UI Symbol", "Segoe UI";
            font-size: 10pt;
            font-weight: 700;
            min-height: 34px;
            min-width: 42px;
            padding: 3px 8px;
        }

        QPushButton[buttonRole="move"]:hover {
            background-color: #eaf2ff;
            border-color: #2563eb;
        }

        QPushButton[buttonRole="move"]:pressed {
            background-color: #dbeafe;
        }

        QPushButton[buttonRole="danger"] {
            background-color: #b91c1c;
            border-color: #991b1b;
            color: #ffffff;
        }

        QPushButton[buttonRole="danger"]:hover {
            background-color: #991b1b;
            border-color: #7f1d1d;
        }

        QPushButton[buttonRole="danger"]:pressed {
            background-color: #7f1d1d;
        }

        QPushButton[buttonRole="primary"]:disabled,
        QPushButton[buttonRole="danger"]:disabled {
            background-color: #e5e7eb;
            border-color: #d1d5db;
            color: #9ca3af;
        }

        QPushButton[buttonSize="compact"] {
            min-height: 18px;
            padding: 3px 10px;
        }

        QCheckBox {
            color: #374151;
            spacing: 6px;
        }

        QCheckBox::indicator {
            background-color: #ffffff;
            border: 1px solid #94a3b8;
            border-radius: 4px;
            height: 14px;
            width: 14px;
        }

        QCheckBox::indicator:checked {
            background-color: #15803d;
            border-color: #15803d;
        }

        QCheckBox::indicator:indeterminate {
            background-color: #2563eb;
            border-color: #2563eb;
        }

        QCheckBox::indicator:disabled {
            background-color: #e5e7eb;
            border-color: #cbd5e1;
        }

        QCheckBox::indicator:checked:disabled {
            background-color: #15803d;
            border-color: #15803d;
        }

        QCheckBox::indicator:indeterminate:disabled {
            background-color: #2563eb;
            border-color: #2563eb;
        }

        QProgressBar {
            background-color: #eef2f7;
            border: 1px solid #cfd8e3;
            border-radius: 7px;
            color: #1f2937;
            font-weight: 700;
            min-height: 22px;
            min-width: 120px;
            text-align: center;
        }

        QProgressBar::chunk {
            background-color: #15803d;
            border-radius: 5px;
            margin: 2px;
        }

        QTextBrowser,
        QWidget#picoRaw {
            background-color: #ffffff;
            border: 1px solid #cfd8e3;
            border-radius: 8px;
            color: #374151;
        }

        QChartView,
        QChartView::viewport {
            background-color: #ffffff;
            border: none;
        }
    )qss";

    setStyleSheet(style);

    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QBrush(QColor("#ffffff")));
    chart->setMargins(QMargins(8, 8, 8, 8));

    series->setColor(QColor("#2563eb"));

    const QColor axisColor("#64748b");
    const QColor gridColor("#e5e7eb");
    QFont axisFont;
    axisFont.setPointSize(8);

    for (QValueAxis *axis : {axisX, axisY}) {
        axis->setLinePenColor(axisColor);
        axis->setGridLineColor(gridColor);
        axis->setLabelsColor(axisColor);
        axis->setLabelsFont(axisFont);
        axis->setTitleBrush(QBrush(QColor("#374151")));
    }
}

void MainWindow::bindConnection(QPushButton *btn, QCheckBox *stat, Device *device){
    QTimer::singleShot(0, btn, &QPushButton::click);
    connect(btn, &QPushButton::clicked, this, [this, btn, stat, device](){
        if (scan.isRunning()) return;
        btn->setEnabled(false);
        ui->scan->setEnabled(false);
        bool connection;
        if (stat->text() == "Offline") {
            stat->setText("connecting");
            connection = true;
        } else {
            stat->setText("disconnecting");
            connection = false;
        }

        stat->setCheckState(Qt::PartiallyChecked);
        QApplication::processEvents();
        DeviceState deviceState = device->connect(connection);

        if (deviceState == online || deviceState == ready) {
            stat->setText("Online");
            stat->setCheckState(Qt::Checked);
            btn->setText("Disconnect");
        } else {
            stat->setText("Offline");
            stat->setCheckState(Qt::Unchecked);
            btn->setText("Connect");
        }
        btn->setEnabled(true);
        ui->scan->setEnabled(true);
    });
}

void MainWindow::bindLine(QLineEdit *line, const QString &key, Device *device, QString &str){
    str = settings.value(key, "").toString();
    line->setText(str);
    connect(line, &QLineEdit::textChanged, this, [this, key, device, &str](const QString &txt){
        if (scan.isRunning()) return;
        str = txt;
#ifndef Q_OS_WASM
        settings.setValue(key, str);
#endif
        checkConfig(device);
        qDebug() << key << ": " << str;
    });
}

void MainWindow::bindSpinBox(QSpinBox *spinBox, const QString &key, Device *device, int &num){
    num = settings.value(key, spinBox->value()).toInt();
    spinBox->setValue(num);
    num = spinBox->value();
#ifndef Q_OS_WASM
    settings.setValue(key, num);
#endif
    connect(spinBox, &QSpinBox::valueChanged, this, [this, key, device, &num](int val) {
        if (scan.isRunning()) return;
        num = val;
#ifndef Q_OS_WASM
        settings.setValue(key, num);
#endif
        checkConfig(device);
        qDebug() << key << ": " << num;
    });
}

void MainWindow::bindDoubleBox(QDoubleSpinBox *spinBox, const QString &key, Device *device, double &num){
    num = settings.value(key, spinBox->value()).toDouble();
    spinBox->setValue(num);
    num = spinBox->value();
#ifndef Q_OS_WASM
    settings.setValue(key, num);
#endif
    connect(spinBox, &QDoubleSpinBox::valueChanged, this, [this, key, device, &num](double val) {
        if (scan.isRunning()) return;
        num = val;
#ifndef Q_OS_WASM
        settings.setValue(key, num);
#endif
        checkConfig(device);
        qDebug() << key << ": " << num;
    });
}

void MainWindow::bindCoordBox(QDoubleSpinBox *coordBox, const QString &key, Device *device, int &num){
    const int defaultSteps = inchToSteps(coordBox->value());
    num = settings.value(key, defaultSteps).toInt();
    coordBox->setValue(num*STEP_SIZE);
    num = inchToSteps(coordBox->value());
#ifndef Q_OS_WASM
    settings.setValue(key, num);
#endif
    connect(coordBox, &QDoubleSpinBox::valueChanged, this, [this, key, device, &num](double val) {
        if (scan.isRunning()) return;
        num = inchToSteps(val);
#ifndef Q_OS_WASM
        settings.setValue(key, num);
#endif
        checkConfig(device);
        qDebug() << key << ": " << num;
    });
}

void MainWindow::bindMove(QPushButton *btn, int dx, int dy, int dz){
    connect(btn, &QPushButton::clicked, this, [this, btn, dx, dy, dz](){
        btn->setEnabled(false);
        ui->scan->setEnabled(false);
        QApplication::processEvents();
        Coord goal;
        goal.X = dx * vmx.steps + vmx.pos.X;
        goal.Y = dy * vmx.steps + vmx.pos.Y;
        goal.Z = dz * vmx.steps + vmx.pos.Z;
        vmx.killflag = false;
        vmx.move(goal);
        emit vmx.updateCoord(vmx.pos);
        btn->setEnabled(true);
        ui->scan->setEnabled(true);
    });
};

void MainWindow::bindComboBox(QComboBox *comboBox, const QString &key, Device *device, int &idx){
    idx = settings.value(key, 0).toInt();
    comboBox->setCurrentIndex(idx);
    connect(comboBox, &QComboBox::currentIndexChanged, this, [this, key, device, &idx](int val){
        if (scan.isRunning()) return;
        idx = val;
#ifndef Q_OS_WASM
        settings.setValue(key, idx);
#endif
        checkConfig(device);
        qDebug() << key << ": " << idx;
    });
};



void MainWindow::checkConfig(Device *device)
{
    if (device != nullptr && device->deviceState == ready) {
        device->deviceState = online;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (scan.isRunning()) {
        event->ignore();
        connect(&scan, &QThread::finished, this, &QWidget::close, Qt::UniqueConnection);
        vmx.kill();
        return;
    }
    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow(){
    if (scan.isRunning()) vmx.kill();
    scan.wait();
    delete ui;
}
