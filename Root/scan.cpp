#include "scan.h"

#include <cmath>

#include "filer.h"
#include "fg.h"
#include "pico.h"
#include "vmx.h"

Scan::Scan(Pico &pico, Fg &fg, Vmx &vmx, const QDir &folderPath)
    : pico(pico)
    , fg(fg)
    , vmx(vmx)
    , folderPath(folderPath)
{}

Scan::Outcome Scan::checkBoundaries(const Coord &minimum, const Coord &maximum, const ProgressCallback &progress)
{
    if (pico.deviceState != ready || fg.deviceState != ready || vmx.deviceState != ready) {
        qWarning() << "Configure all devices before scanning";
        return Outcome::Failed;
    }

    if (vmx.steps <= 0 || maximum.X < minimum.X || maximum.Y < minimum.Y || maximum.Z < minimum.Z) {
        qWarning() << "Invalid scan bounds or step size";
        return Outcome::Failed;
    }

    int trajectoryStep = vmx.steps;
#ifdef Q_OS_WASM
    trajectoryStep = simulatedScanStep(minimum, maximum, vmx.steps);
#else
    if (!isValidScanRequest(minimum, maximum, vmx.steps)) {
        qWarning() << "Scan trajectory exceeds the supported point count";
        return Outcome::Failed;
    }
#endif

    const QVector<Coord> trajectory = buildScanTrajectory(minimum, maximum, trajectoryStep);
    if (trajectory.isEmpty()) {
        qWarning() << "Scan trajectory is empty";
        return Outcome::Failed;
    }

    this->minimum = minimum;
    this->maximum = maximum;
    points = trajectory;
    peaks.clear();
    peaks.reserve(points.size());
    nextPoint = 0;
    focusPeak = -1.0;
    focusPosition = points.first();

    progress("Checking boundary", 0, 0, {});
    vmx.move(maximum);
    vmx.coord();
    if (wasCancelled()) {
        reset();
        return Outcome::Cancelled;
    }

    vmx.move(minimum);
    vmx.coord();
    if (wasCancelled()) {
        reset();
        return Outcome::Cancelled;
    }

    boundaryChecked = true;
    return Outcome::Ready;
}

Scan::Outcome Scan::prepare(const Coord &minimum, const Coord &maximum)
{
    Q_UNUSED(minimum)
    Q_UNUSED(maximum)
    if (!boundaryChecked || points.isEmpty() || pico.deviceState != ready || fg.deviceState != ready || vmx.deviceState != ready) {
        qWarning() << "Configure all devices before scanning";
        reset();
        return Outcome::Failed;
    }
    return Outcome::Scanning;
}

Scan::Outcome Scan::acquireNext(const ProgressCallback &progress)
{
    if (nextPoint >= points.size()) {
        return Outcome::Failed;
    }
    if (pico.deviceState != ready || fg.deviceState != ready || vmx.deviceState != ready) {
        qWarning() << "A device was reconfigured during scanning";
        reset();
        return Outcome::Failed;
    }

    const Coord &point = points.at(nextPoint);
    vmx.move(point);
    if (wasCancelled()) {
        reset();
        return Outcome::Cancelled;
    }

    pico.setSimulationGain(simulationGain(point, minimum, maximum));
    pico.setSimulationTimeShift(simulationPulseTimeShift(point, minimum, maximum));
    pico.runBlock();
    fg.trig();
    const Data data = pico.read();
    peaks.append(data.peak);
    if (data.peak > focusPeak) {
        focusPeak = data.peak;
        focusPosition = point;
    }
    ++nextPoint;
    progress("Scanning", nextPoint, points.size(), data);

    if (nextPoint < points.size()) {
        return Outcome::Scanning;
    }

    if (!Filer::saveScan(folderPath, {fg.freq, fg.amp, volt[pico.range], pico.samp, vmx.pos}, points, peaks)) {
        reset();
        return Outcome::Failed;
    }

    vmx.move(focusPosition);
    vmx.coord();
    resetSimulation();
    pico.runBlock();
    fg.trig();
    boundaryChecked = false;
    points.clear();
    peaks.clear();
    nextPoint = 0;
    return Outcome::Completed;
}

int Scan::pointCount() const
{
    return points.size();
}

void Scan::reset()
{
    resetSimulation();
    vmx.killFlag = false;
    boundaryChecked = false;
    points.clear();
    peaks.clear();
    nextPoint = 0;
    focusPeak = -1.0;
}

void Scan::resetSimulation()
{
    pico.setSimulationGain(1.0);
    pico.setSimulationTimeShift(0.0);
}

double Scan::simulationGain(const Coord &point, const Coord &minimum, const Coord &maximum)
{
    const auto normalizedDistance = [](int value, int lower, int upper) {
        if (upper == lower) {
            return 0.0;
        }
        const double centre = (static_cast<double>(lower) + static_cast<double>(upper)) / 2.0;
        const double radius = (static_cast<double>(upper) - static_cast<double>(lower)) / 2.0;
        return (static_cast<double>(value) - centre) / radius;
    };

    const double x = normalizedDistance(point.X, minimum.X, maximum.X);
    const double y = normalizedDistance(point.Y, minimum.Y, maximum.Y);
    const double z = normalizedDistance(point.Z, minimum.Z, maximum.Z);
    return std::exp(-2.5 * ((x * x) + (y * y) + (z * z)));
}

bool Scan::wasCancelled()
{
    if (!vmx.killFlag) {
        return false;
    }
    qWarning() << "Scan cancelled";
    vmx.killFlag = false;
    return true;
}
