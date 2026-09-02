#include <QBuffer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "filer.h"
#include "scan.h"

class ScanUtilitiesTest final : public QObject
{
    Q_OBJECT

private slots:
    void trajectoryVisitsRowsInSerpentineOrder();
    void scanPointCountMatchesTheGridWithoutBuildingIt();
    void scanRequestValidationRejectsInvalidBoundsAtTheStart();
    void coordinateStepConversionPreservesTheControlDefault();
    void simulatedScanStepCapsTheGridAtFivePointsPerAxis();
    void trajectoryRejectsNonPositiveStep();
    void simulationPulseIsDelayedAwayFromFocus();
    void simulationPulseShiftDoesNotSaturateAtScanCorners();
    void scanControlFollowsTheRequestedPauseSequence();
    void scanTimerSpacingTargetsFiveSeconds();
    void exportPathSkipsExistingFiles();
    void metadataUsesTheEstablishedCsvSchema();
    void triggerSaveWritesCaptureCsv();
    void scanSaveWritesPressureCsv();
};

void ScanUtilitiesTest::trajectoryVisitsRowsInSerpentineOrder()
{
    const QVector<Coord> points = buildScanTrajectory({0, 0, 0}, {2, 1, 0}, 1);

    QCOMPARE(points.size(), 6);
    QCOMPARE(points.at(0).X, 0);
    QCOMPARE(points.at(2).X, 2);
    QCOMPARE(points.at(3).X, 2);
    QCOMPARE(points.at(5).X, 0);
}

void ScanUtilitiesTest::scanPointCountMatchesTheGridWithoutBuildingIt()
{
    QCOMPARE(scanPointCount({0, 0, 0}, {2, 1, 0}, 1), 6);
    QCOMPARE(scanPointCount({0, 0, 0}, {10, 10, 10}, 5), 27);
}

void ScanUtilitiesTest::scanRequestValidationRejectsInvalidBoundsAtTheStart()
{
    QVERIFY(isValidScanRequest({0, 0, 0}, {2, 1, 0}, 1));
    QVERIFY(!isValidScanRequest({2, 0, 0}, {0, 1, 0}, 1));
    QVERIFY(!isValidScanRequest({0, 0, 0}, {2, 1, 0}, 0));
}

void ScanUtilitiesTest::coordinateStepConversionPreservesTheControlDefault()
{
    QCOMPARE(coordinateToSteps(0.1), 400);
    QCOMPARE(coordinateToSteps(STEP_SIZE), 1);
}

void ScanUtilitiesTest::simulatedScanStepCapsTheGridAtFivePointsPerAxis()
{
    const Coord minimum{0, 0, 0};
    const Coord maximum{100, 100, 100};
    QCOMPARE(simulatedScanStep(minimum, maximum, 1), 25);
    QCOMPARE(buildScanTrajectory(minimum, maximum, simulatedScanStep(minimum, maximum, 1)).size(), 125);
}

void ScanUtilitiesTest::trajectoryRejectsNonPositiveStep()
{
    QVERIFY(buildScanTrajectory({0, 0, 0}, {1, 1, 1}, 0).isEmpty());
}

void ScanUtilitiesTest::simulationPulseIsDelayedAwayFromFocus()
{
    const Coord minimum{-10, -10, 0};
    const Coord maximum{10, 10, 0};

    QCOMPARE(simulationPulseTimeShift({0, 0, 0}, minimum, maximum), 0.0);
    QVERIFY(simulationPulseTimeShift({10, 0, 0}, minimum, maximum) > 0.0);
    QVERIFY(simulationPulseTimeShift({10, 10, 0}, minimum, maximum)
            > simulationPulseTimeShift({10, 0, 0}, minimum, maximum));
}

void ScanUtilitiesTest::simulationPulseShiftDoesNotSaturateAtScanCorners()
{
    const Coord minimum{-10, -10, -10};
    const Coord maximum{10, 10, 10};

    QCOMPARE(simulationPulseTimeShift({10, 10, 10}, minimum, maximum), 160.0);
}

void ScanUtilitiesTest::scanControlFollowsTheRequestedPauseSequence()
{
    ScanControlState state = ScanControlState::Idle;

    state = transitionScanState(state, ScanControlEvent::Start);
    QCOMPARE(state, ScanControlState::CheckingBoundary);
    state = transitionScanState(state, ScanControlEvent::BoundariesChecked);
    QCOMPARE(state, ScanControlState::Ready);
    state = transitionScanState(state, ScanControlEvent::Continue);
    QCOMPARE(state, ScanControlState::Scanning);
    state = transitionScanState(state, ScanControlEvent::Pause);
    QCOMPARE(state, ScanControlState::Paused);
    state = transitionScanState(state, ScanControlEvent::Continue);
    QCOMPARE(state, ScanControlState::Scanning);
    state = transitionScanState(state, ScanControlEvent::Complete);
    QCOMPARE(state, ScanControlState::Idle);
}

void ScanUtilitiesTest::scanTimerSpacingTargetsFiveSeconds()
{
    QCOMPARE(scanTimerIntervalMilliseconds(5), 1000);
    QCOMPARE(scanTimerIntervalMilliseconds(3), 1667);
    QCOMPARE(scanTimerIntervalMilliseconds(1), 5000);
}

void ScanUtilitiesTest::exportPathSkipsExistingFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFile(directory.filePath("Trig.csv")).open(QIODevice::WriteOnly));
    QVERIFY(QFile(directory.filePath("Trig_1.csv")).open(QIODevice::WriteOnly));

    QCOMPARE(Filer::nextAvailableCsvPath(QDir(directory.path()), "Trig"),
             directory.filePath("Trig_2.csv"));
}

void ScanUtilitiesTest::metadataUsesTheEstablishedCsvSchema()
{
    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::ReadWrite));
    QTextStream stream(&buffer);

    Filer::writeCaptureMetadata(stream, {500, 120, 5000, 1000, {1, 2, 3}});
    stream.flush();

    QCOMPARE(buffer.data(), QByteArray("Freq(kHz),500\n"
                                       "Amp(mVpp),120\n"
                                       "Range(mVpp),5000\n"
                                       "SampleCount,1000\n"
                                       "X,1\n"
                                       "Y,2\n"
                                       "Z,3\n"));
}

void ScanUtilitiesTest::triggerSaveWritesCaptureCsv()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const Filer::CaptureMetadata metadata{500, 120, 5000, 1000, {1, 2, 3}};
    const QVector<QPointF> waveform{{0.0, 1.25}, {1.0, -2.5}};
    QVERIFY(Filer::saveTrigger(QDir(directory.path()), metadata, SENS * 10.0, waveform));

    QFile file(directory.filePath("Trig.csv"));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.startsWith("Freq(kHz),500\nAmp(mVpp),120\n"));
    QVERIFY(contents.contains("P(kPa),10\n\n Points \n1.25\n-2.5\n"));
}

void ScanUtilitiesTest::scanSaveWritesPressureCsv()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const Filer::CaptureMetadata metadata{500, 120, 5000, 1000, {1, 2, 3}};
    const QVector<Coord> points{{0, 0, 0}, {1, 0, 0}};
    const QVector<double> peaks{SENS * 10.0, SENS * 20.0};
    QVERIFY(Filer::saveScan(QDir(directory.path()), metadata, points, peaks));

    QFile file(directory.filePath("Scan.csv"));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.startsWith("Freq(kHz),500\nAmp(mVpp),120\n"));
    QVERIFY(contents.contains("\n Pressures(kPa) \n0,0,0,10\n1,0,0,20\n"));
}

QTEST_APPLESS_MAIN(ScanUtilitiesTest)

#include "tst_scanutilities.moc"
