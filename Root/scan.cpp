#include "scan.h"

Scan::Scan(Pico &pico, Fg &fg, Vmx &vmx) : pico(pico), fg(fg), vmx(vmx)
{
    connect(&vmx, &Vmx::killed, this, [this] {
        {
            QMutexLocker lock(&mutex);
            wake.wakeAll();
        }
        if (!isRunning() && state == Paused) {
            state = Killed;
            emit stateChanged(Killed);
        }
    }, Qt::DirectConnection);
    connect(this, &QThread::finished, this, [this] { emit stateChanged(state); });
}

Scan::~Scan()
{
    if (isRunning()) vmx.kill();
    wait();
}

void Scan::checkBound(const Coord &min, const Coord &max)
{
    if (QThread::currentThread() != this) {
        if (isRunning()) return;
        vmx.killflag = false;
        minimum = min;
        maximum = max;
        points.clear();
        peaks.clear();
        state = Checking;
        pico.moveToThread(this);
        fg.moveToThread(this);
        vmx.moveToThread(this);
        QThread::start();
        emit stateChanged(Checking);
        return;
    }
    const qint64 dx = static_cast<qint64>(max.X) - min.X;
    const qint64 dy = static_cast<qint64>(max.Y) - min.Y;
    const qint64 dz = static_cast<qint64>(max.Z) - min.Z;
    const qint64 step = vmx.steps;
    if (step <= 0 || dx < 0 || dy < 0 || dz < 0) {
        qWarning() << "Invalid scan bounds or step size";
        state = Error;
        emit stateChanged(Error);
        return;
    }
    qint64 count = 1;
    for (qint64 span : {dx, dy, dz}) {
        const qint64 axisCount = span / step + 1;
        if (count > std::numeric_limits<int>::max() / axisCount) {
            qWarning() << "Scan trajectory exceeds the supported point count";
            state = Error;
            emit stateChanged(Error);
            return;
        }
        count *= axisCount;
    }
    points.reserve(count);
    peaks.reserve(count);
    for (qint64 z = min.Z; z <= max.Z; z += step) {
        bool increasingX = true;
        for (qint64 y = min.Y; y <= max.Y; y += step) {
            for (qint64 x = 0; x <= dx; x += step) {
                if (vmx.killflag) return;
                points.append({static_cast<int>(increasingX ? min.X + x : max.X - x),
                               static_cast<int>(y), static_cast<int>(z)});
            }
            increasingX = !increasingX;
        }
    }
    for (const Coord &corner : {max, min}) {
        if (vmx.killflag) return;
        vmx.move(corner);
        if (vmx.killflag) return;
        vmx.coord();
    }
}

void Scan::resume()
{
    {
        QMutexLocker lock(&mutex);
        if (vmx.killflag || (state == Paused && isRunning())) return;
        if (state != Ready && state != Paused) return;
        state = Scanning;
        if (!isRunning()) {
            pico.moveToThread(this);
            fg.moveToThread(this);
            vmx.moveToThread(this);
            QThread::start();
        } else {
            wake.wakeAll();
        }
    }
    emit stateChanged(Scanning);
}

void Scan::pause()
{
    {
        QMutexLocker lock(&mutex);
        if (state != Scanning || !isRunning()) return;
        state = Paused;
    }
    emit stateChanged(Paused);
}

void Scan::run()
{
    if (state == Checking) {
        checkBound(minimum, maximum);
        if (state == Checking && !vmx.killflag) {
            state = Ready;
            emit stateChanged(Ready);
        }
    }
    while (!vmx.killflag) {
        {
            QMutexLocker lock(&mutex);
            while (state == Ready && !vmx.killflag) wake.wait(&mutex);
            if (vmx.killflag || state != Scanning) break;
        }
        if (peaks.size() == points.size()) {
#ifndef Q_OS_WASM
            if (!Filer::saveScan({fg.freq, fg.amp, volt[pico.range], pico.samp, vmx.pos, pico.sens}, points, peaks)) {
                {
                    QMutexLocker lock(&mutex);
                    state = Error;
                }
                emit stateChanged(Error);
                break;
            }
#endif
            double focusPeak = -1.0;
            int focus = 0;
            for (int i = 0; i < peaks.size(); ++i) {
                if (peaks.at(i) > focusPeak) {
                    focusPeak = peaks.at(i);
                    focus = i;
                }
            }
            if (vmx.killflag) break;
            vmx.move(points.at(focus));
            if (vmx.killflag) break;
            vmx.coord();
            if (vmx.killflag) break;
            pico.runBlock();
            fg.trig();
            {
                QMutexLocker lock(&mutex);
                state = Idle;
            }
            break;
        }
        vmx.move(points.at(peaks.size()));
        if (vmx.killflag) break;
        pico.runBlock();
        fg.trig();
        const Data data = pico.read();
        if (vmx.killflag) break;
        peaks.append(data.peak);
        emit measured(data, peaks.size(), points.size());
    }
    if (vmx.killflag) {
        {
            QMutexLocker lock(&mutex);
            state = Killed;
        }
        emit stateChanged(Killed);
        vmx.kill();
        vmx.coord();
    }
    // Return device ownership before the window enables manual controls again.
    pico.moveToThread(thread());
    fg.moveToThread(thread());
    vmx.moveToThread(thread());
}
