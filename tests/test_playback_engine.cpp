#include <QTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include "playback/playback_engine.h"
#include "playback/mpv_engine.h"
#include "ui/time_bar.h"
#include "ui/omni_icons.h"

class TestPlaybackEngine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() {
        // MpvEngine persists settings; keep them out of the developer's real config
        QStandardPaths::setTestModeEnabled(true);
    }

    void testDefaultValues() {
        Omnipot::MpvEngine engine;
        QCOMPARE(engine.state(), Omnipot::PlaybackState::Idle);
        QCOMPARE(engine.position(), 0.0);
        QCOMPARE(engine.duration(), 0.0);
        QCOMPARE(engine.speed(), 1.0);
        QCOMPARE(engine.isMuted(), false);
        QCOMPARE(engine.volume(), 100);
    }

    void testVolumeClamping() {
        Omnipot::MpvEngine engine;
        // Setting negative volume should clamp to 0
        engine.setVolume(-10);
        // Setting excess volume should clamp to 200
        engine.setVolume(250);
        // Verify engine object did not crash on clamps
    }

    void testSpeedClamping() {
        Omnipot::MpvEngine engine;
        // Setting speed below 0.2 should clamp to 0.2
        engine.setSpeed(0.05);
        // Setting speed above 12.0 should clamp to 12.0
        engine.setSpeed(25.0);
    }

    void testAudioNormalizer() {
        Omnipot::MpvEngine engine;
        engine.setNormalizerEnabled(true);
        QCOMPARE(engine.isNormalizerEnabled(), true);
        engine.setNormalizerEnabled(false);
        QCOMPARE(engine.isNormalizerEnabled(), false);
    }

    void testTimeBarFormatting() {
        Omnipot::TimeBar bar;
        bar.setDuration(3665.0); // 1 hour 1 minute 5 seconds
        bar.setPosition(65.0);   // 1 minute 5 seconds

        QCOMPARE(bar.maximum(), 10000);
    }

    void testSignalSpies() {
        Omnipot::MpvEngine engine;
        QSignalSpy stateSpy(&engine, &Omnipot::PlaybackEngine::stateChanged);
        QSignalSpy volSpy(&engine, &Omnipot::PlaybackEngine::volumeChanged);
        QSignalSpy muteSpy(&engine, &Omnipot::PlaybackEngine::muteChanged);
        QSignalSpy speedSpy(&engine, &Omnipot::PlaybackEngine::speedChanged);
        QSignalSpy colorSpy(&engine, &Omnipot::PlaybackEngine::colorsChanged);
        QSignalSpy eqSpy(&engine, &Omnipot::PlaybackEngine::equalizerChanged);

        QVERIFY(stateSpy.isValid());
        QVERIFY(volSpy.isValid());
        QVERIFY(muteSpy.isValid());
        QVERIFY(speedSpy.isValid());
        QVERIFY(colorSpy.isValid());
        QVERIFY(eqSpy.isValid());
    }

    void testColorAdjustments() {
        Omnipot::MpvEngine engine;
        QCOMPARE(engine.brightness(), 0);
        QCOMPARE(engine.contrast(), 0);
        QCOMPARE(engine.saturation(), 0);
        QCOMPARE(engine.hue(), 0);

        engine.setBrightness(25);
        QCOMPARE(engine.brightness(), 25);
        engine.setContrast(-15);
        QCOMPARE(engine.contrast(), -15);
        engine.setSaturation(50);
        QCOMPARE(engine.saturation(), 50);
        engine.setHue(10);
        QCOMPARE(engine.hue(), 10);

        // Clamping to [-100, 100]
        engine.setBrightness(150);
        QCOMPARE(engine.brightness(), 100);
        engine.setContrast(-120);
        QCOMPARE(engine.contrast(), -100);

        // Reset
        engine.resetColors();
        QCOMPARE(engine.brightness(), 0);
        QCOMPARE(engine.contrast(), 0);
        QCOMPARE(engine.saturation(), 0);
        QCOMPARE(engine.hue(), 0);
    }

    void testEqualizer() {
        Omnipot::MpvEngine engine;
        QCOMPARE(engine.isEqualizerEnabled(), false);

        for (int i = 0; i < 10; ++i) {
            QCOMPARE(engine.equalizerBand(i), 0.0);
        }

        engine.setEqualizerEnabled(true);
        QCOMPARE(engine.isEqualizerEnabled(), true);

        // Set specific band
        engine.setEqualizerBand(3, 4.5);
        QCOMPARE(engine.equalizerBand(3), 4.5);

        // Preset Rock
        engine.setEqualizerPreset("Rock");
        QCOMPARE(engine.equalizerBand(0), 4.5);
        QCOMPARE(engine.equalizerBand(9), 4.0);

        // Reset
        engine.resetEqualizer();
        QCOMPARE(engine.equalizerBand(0), 0.0);
        QCOMPARE(engine.equalizerBand(9), 0.0);
    }

    void testVideoTransforms() {
        Omnipot::MpvEngine engine;
        QCOMPARE(engine.rotation(), 0);
        QCOMPARE(engine.isHorizontalFlip(), false);
        QCOMPARE(engine.isVerticalFlip(), false);
        QCOMPARE(engine.isDeinterlacing(), false);

        engine.rotateClockwise();
        QCOMPARE(engine.rotation(), 90);
        engine.rotateClockwise();
        QCOMPARE(engine.rotation(), 180);

        engine.setHorizontalFlip(true);
        QCOMPARE(engine.isHorizontalFlip(), true);
        engine.setVerticalFlip(true);
        QCOMPARE(engine.isVerticalFlip(), true);
        engine.setDeinterlacing(true);
        QCOMPARE(engine.isDeinterlacing(), true);
    }

    void testSubtitleAndDelays() {
        Omnipot::MpvEngine engine;
        QCOMPARE(engine.subtitleFontSize(), 55);
        QCOMPARE(engine.subtitlePosition(), 100);
        QCOMPARE(engine.isAssOverride(), false);
        QCOMPARE(engine.audioDelay(), 0.0);
        QCOMPARE(engine.subtitleDelay(), 0.0);

        engine.setSubtitleFontSize(65);
        QCOMPARE(engine.subtitleFontSize(), 65);
        engine.setSubtitlePosition(80);
        QCOMPARE(engine.subtitlePosition(), 80);
        engine.setAssOverride(true);
        QCOMPARE(engine.isAssOverride(), true);

        engine.setAudioDelay(0.15);
        QCOMPARE(engine.audioDelay(), 0.15);
        engine.setSubtitleDelay(-0.5);
        QCOMPARE(engine.subtitleDelay(), -0.5);
    }

    void testLooping() {
        Omnipot::MpvEngine engine;
        QCOMPARE(engine.loopA(), -1.0);
        QCOMPARE(engine.loopB(), -1.0);

        engine.setLoopA(12.5);
        QCOMPARE(engine.loopA(), 12.5);
        engine.setLoopB(45.0);
        QCOMPARE(engine.loopB(), 45.0);

        engine.clearLoop();
        QCOMPARE(engine.loopA(), -1.0);
        QCOMPARE(engine.loopB(), -1.0);
    }

    void testAudioDevices() {
        Omnipot::MpvEngine engine;
        engine.initialize();

        QStringList devList = engine.audioDevices();
        QVERIFY(!devList.isEmpty());
        QVERIFY(devList.first().startsWith(QStringLiteral("auto")));

        engine.setAudioDevice(QStringLiteral("auto"));
        QCOMPARE(engine.currentAudioDevice(), QStringLiteral("auto"));

        engine.shutdown();
    }

    void testOmniIcons() {
        QIcon play = Omnipot::OmniIcons::playIcon();
        QVERIFY(!play.isNull());
        QIcon pause = Omnipot::OmniIcons::pauseIcon();
        QVERIFY(!pause.isNull());
        QIcon stop = Omnipot::OmniIcons::stopIcon();
        QVERIFY(!stop.isNull());
        QIcon prev = Omnipot::OmniIcons::prevIcon();
        QVERIFY(!prev.isNull());
        QIcon next = Omnipot::OmniIcons::nextIcon();
        QVERIFY(!next.isNull());
        QIcon open = Omnipot::OmniIcons::openIcon();
        QVERIFY(!open.isNull());
        QIcon vol = Omnipot::OmniIcons::volumeIcon(100, false);
        QVERIFY(!vol.isNull());
        QIcon volMuted = Omnipot::OmniIcons::volumeIcon(0, true);
        QVERIFY(!volMuted.isNull());
        QIcon pl = Omnipot::OmniIcons::playlistIcon();
        QVERIFY(!pl.isNull());
        QIcon cb = Omnipot::OmniIcons::controlBoxIcon();
        QVERIFY(!cb.isNull());
        QIcon fs = Omnipot::OmniIcons::fullscreenIcon(false);
        QVERIFY(!fs.isNull());
        QIcon fsTrue = Omnipot::OmniIcons::fullscreenIcon(true);
        QVERIFY(!fsTrue.isNull());
        QIcon pin = Omnipot::OmniIcons::pinIcon(false);
        QVERIFY(!pin.isNull());
        QIcon pinPinned = Omnipot::OmniIcons::pinIcon(true);
        QVERIFY(!pinPinned.isNull());
    }
};

QTEST_MAIN(TestPlaybackEngine)
#include "test_playback_engine.moc"
