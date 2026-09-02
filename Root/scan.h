#pragma once

#include <QDir>
#include <QFile>
#include <QVector>

#include <cmath>
#include <algorithm>
#include <functional>
#include <limits>

#include "vmx.h"

struct Data;
class Fg;
class Pico;

enum class ScanControlState {
    Idle,
    CheckingBoundary,
    Ready,
    Scanning,
    Paused
};

enum class ScanControlEvent {
    Start,
    BoundariesChecked,
    Continue,
    Pause,
    Complete,
    Cancel,
    Fail
};

inline ScanControlState transitionScanState(ScanControlState state, ScanControlEvent event)
{
    if (event == ScanControlEvent::Complete || event == ScanControlEvent::Cancel || event == ScanControlEvent::Fail) {
        return ScanControlState::Idle;
    }

    switch (state) {
    case ScanControlState::Idle:
        return event == ScanControlEvent::Start ? ScanControlState::CheckingBoundary : state;
    case ScanControlState::CheckingBoundary:
        return event == ScanControlEvent::BoundariesChecked ? ScanControlState::Ready : state;
    case ScanControlState::Ready:
        return event == ScanControlEvent::Continue ? ScanControlState::Scanning : state;
    case ScanControlState::Scanning:
        return event == ScanControlEvent::Pause ? ScanControlState::Paused : state;
    case ScanControlState::Paused:
        return event == ScanControlEvent::Continue ? ScanControlState::Scanning : state;
    }
    return state;
}

inline int scanTimerIntervalMilliseconds(int pointCount)
{
    constexpr double scanDurationMilliseconds = 5000.0;
    return pointCount > 0
        ? static_cast<int>(std::ceil(scanDurationMilliseconds / pointCount))
        : 0;
}

inline int scanPointCount(const Coord &minimum, const Coord &maximum, int step)
{
    if (step <= 0 || maximum.X < minimum.X || maximum.Y < minimum.Y || maximum.Z < minimum.Z) {
        return 0;
    }

    const qint64 xCount = (static_cast<qint64>(maximum.X) - minimum.X) / step + 1;
    const qint64 yCount = (static_cast<qint64>(maximum.Y) - minimum.Y) / step + 1;
    const qint64 zCount = (static_cast<qint64>(maximum.Z) - minimum.Z) / step + 1;
    const qint64 pointCount = xCount * yCount * zCount;
    return pointCount <= std::numeric_limits<int>::max() ? static_cast<int>(pointCount) : 0;
}

inline bool isValidScanRequest(const Coord &minimum, const Coord &maximum, int step)
{
    return scanPointCount(minimum, maximum, step) > 0;
}

inline int simulatedScanStep(const Coord &minimum, const Coord &maximum, int requestedStep)
{
    constexpr qint64 maximumPointsPerAxis = 5;
    if (requestedStep <= 0 || maximum.X < minimum.X || maximum.Y < minimum.Y || maximum.Z < minimum.Z) {
        return requestedStep;
    }

    const auto stepForAxis = [](int lower, int upper) {
        const qint64 range = static_cast<qint64>(upper) - lower;
        const qint64 sampledStep = (range + maximumPointsPerAxis - 2) / (maximumPointsPerAxis - 1);
        return static_cast<int>(std::min(sampledStep, static_cast<qint64>(std::numeric_limits<int>::max())));
    };
    return std::max({requestedStep,
                     stepForAxis(minimum.X, maximum.X),
                     stepForAxis(minimum.Y, maximum.Y),
                     stepForAxis(minimum.Z, maximum.Z)});
}

class Scan
{
public:
    enum class Outcome {
        Ready,
        Scanning,
        Completed,
        Cancelled,
        Failed
    };

    using ProgressCallback = std::function<void(const QString &, int, int, const Data &)>;

    Scan(Pico &pico, Fg &fg, Vmx &vmx, const QDir &folderPath);
    Outcome checkBoundaries(const Coord &minimum, const Coord &maximum, const ProgressCallback &progress);
    Outcome prepare(const Coord &minimum, const Coord &maximum);
    Outcome acquireNext(const ProgressCallback &progress);
    int pointCount() const;
    void reset();

private:
    static double simulationGain(const Coord &point, const Coord &minimum, const Coord &maximum);
    bool wasCancelled();
    void resetSimulation();
    Pico &pico;
    Fg &fg;
    Vmx &vmx;
    QDir folderPath;
    bool boundaryChecked = false;
    Coord minimum;
    Coord maximum;
    QVector<Coord> points;
    QVector<double> peaks;
    int nextPoint = 0;
    double focusPeak = -1.0;
    Coord focusPosition;
};

inline QVector<Coord> buildScanTrajectory(const Coord &minimum, const Coord &maximum, int step)
{
    if (!isValidScanRequest(minimum, maximum, step)) {
        return {};
    }

    QVector<Coord> points;
    for (int z = minimum.Z; z <= maximum.Z; z += step) {
        bool increasingX = true;
        for (int y = minimum.Y; y <= maximum.Y; y += step) {
            if (increasingX) {
                for (int x = minimum.X; x <= maximum.X; x += step) {
                    points.append({x, y, z});
                }
            } else {
                for (int x = maximum.X; x >= minimum.X; x -= step) {
                    points.append({x, y, z});
                }
            }
            increasingX = !increasingX;
        }
    }
    return points;
}

inline double simulationPulseTimeShift(const Coord &point, const Coord &minimum, const Coord &maximum)
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
    constexpr double maximumShift = 160.0;
    return maximumShift * std::sqrt(((x * x) + (y * y) + (z * z)) / 3.0);
}
