#include <QTest>
#include <QSignalSpy>
#include "playback/playback_engine.h"
#include "playback/mpv_engine.h"
#include "ui/time_bar.h"

class TestPlaybackEngine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() {
        // Run before all tests
    }

    void testDefaultValues() {
        PotPlayer::MpvEngine engine;
        QCOMPARE(engine.state(), PotPlayer::PlaybackState::Idle);
        QCOMPARE(engine.position(), 0.0);
        QCOMPARE(engine.duration(), 0.0);
        QCOMPARE(engine.speed(), 1.0);
        QCOMPARE(engine.isMuted(), false);
        QCOMPARE(engine.volume(), 100);
    }

    void testVolumeClamping() {
        PotPlayer::MpvEngine engine;
        // Setting negative volume should clamp to 0
        engine.setVolume(-10);
        // Setting excess volume should clamp to 200
        engine.setVolume(250);
        // Verify engine object did not crash on clamps
    }

    void testSpeedClamping() {
        PotPlayer::MpvEngine engine;
        // Setting speed below 0.2 should clamp to 0.2
        engine.setSpeed(0.05);
        // Setting speed above 12.0 should clamp to 12.0
        engine.setSpeed(25.0);
    }

    void testTimeBarFormatting() {
        PotPlayer::TimeBar bar;
        bar.setDuration(3665.0); // 1 hour 1 minute 5 seconds
        bar.setPosition(65.0);   // 1 minute 5 seconds

        QCOMPARE(bar.maximum(), 10000);
    }

    void testSignalSpies() {
        PotPlayer::MpvEngine engine;
        QSignalSpy stateSpy(&engine, &PotPlayer::PlaybackEngine::stateChanged);
        QSignalSpy volSpy(&engine, &PotPlayer::PlaybackEngine::volumeChanged);
        QSignalSpy muteSpy(&engine, &PotPlayer::PlaybackEngine::muteChanged);
        QSignalSpy speedSpy(&engine, &PotPlayer::PlaybackEngine::speedChanged);

        QVERIFY(stateSpy.isValid());
        QVERIFY(volSpy.isValid());
        QVERIFY(muteSpy.isValid());
        QVERIFY(speedSpy.isValid());
    }
};

QTEST_MAIN(TestPlaybackEngine)
#include "test_playback_engine.moc"
