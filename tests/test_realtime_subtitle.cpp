#include <QTest>
#include <QStandardPaths>
#include "subtitle/subtitle_timeline.h"
#include "subtitle/whisper_model.h"
#include "subtitle/speech_activity.h"
#include <cmath>
#include <vector>

using Omnipot::SubtitleSegment;
using Omnipot::SubtitleTimeline;
using Omnipot::SpeechActivity;

static constexpr int kRate = 16000;

// Quiet noise with a 440 Hz "voice" between the given times
static std::vector<float> makeAudio(double seconds, double voiceFrom, double voiceTo) {
    std::vector<float> pcm(static_cast<size_t>(seconds * kRate));
    for (size_t i = 0; i < pcm.size(); ++i) {
        const double t = static_cast<double>(i) / kRate;
        const float noise = 0.001f * static_cast<float>((i * 7919) % 100) / 100.0f;
        const float voice = (t >= voiceFrom && t < voiceTo) ? 0.3f * static_cast<float>(std::sin(2.0 * M_PI * 440.0 * t)) : 0.0f;
        pcm[i] = noise + voice;
    }
    return pcm;
}

class TestRealtimeSubtitle : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testFirstWindowStartsAtPlayhead() {
        SubtitleTimeline t;
        auto w = t.nextWindow(12.0, 600.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->start, 12.0);
        QCOMPARE(w->end, 37.0);
    }

    void testContinuesAfterCoveredRange() {
        SubtitleTimeline t;
        t.addResult(0.0, 25.0, {});
        auto w = t.nextWindow(3.0, 600.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->start, 25.0);
    }

    void testStopsAtLookahead() {
        SubtitleTimeline t;
        t.addResult(0.0, 70.0, {});
        QVERIFY(!t.nextWindow(5.0, 600.0, 60.0, 25.0).has_value());
        // Once playback moves on, work resumes
        QVERIFY(t.nextWindow(15.0, 600.0, 60.0, 25.0).has_value());
    }

    void testClampsToDurationAndStopsAtEnd() {
        SubtitleTimeline t;
        auto w = t.nextWindow(590.0, 600.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->end, 600.0);
        t.addResult(w->start, w->end, {});
        QVERIFY(!t.nextWindow(595.0, 600.0, 60.0, 25.0).has_value());
    }

    void testUnknownDurationIsNotClamped() {
        SubtitleTimeline t;
        auto w = t.nextWindow(0.0, 0.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->end, 25.0);
    }

    void testForwardSeekSkipsGap() {
        SubtitleTimeline t;
        t.addResult(0.0, 25.0, {});
        auto w = t.nextWindow(300.0, 600.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->start, 300.0);
    }

    void testBackwardSeekStopsBeforeCoveredRange() {
        SubtitleTimeline t;
        t.addResult(100.0, 160.0, {});
        auto w = t.nextWindow(90.0, 600.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->start, 90.0);
        QCOMPARE(w->end, 100.0);
    }

    void testRangesMergeAcrossTinyGaps() {
        SubtitleTimeline t;
        t.addResult(0.0, 25.0, {});
        t.addResult(25.2, 50.0, {});
        QVERIFY(t.isCovered(25.1));
        auto w = t.nextWindow(10.0, 600.0, 60.0, 25.0);
        QVERIFY(w.has_value());
        QCOMPARE(w->start, 50.0);
    }

    void testSegmentsSortedAndBlankDropped() {
        SubtitleTimeline t;
        t.addResult(25.0, 50.0, {{30.0, 32.0, QStringLiteral("second")}});
        t.addResult(0.0, 25.0, {{1.0, 2.5, QStringLiteral(" first ")}, {3.0, 4.0, QStringLiteral("   ")}});
        QCOMPARE(t.segments().size(), 2);
        QCOMPARE(t.segments().at(0).text, QStringLiteral(" first "));
        QCOMPARE(t.segments().at(1).text, QStringLiteral("second"));
    }

    void testSrtOutput() {
        SubtitleTimeline t;
        t.addResult(0.0, 25.0, {{1.0, 2.5, QStringLiteral(" Hello there. ")}, {3661.25, 3662.0, QStringLiteral("Later")}});
        const QString expected = QStringLiteral(
            "1\n00:00:01,000 --> 00:00:02,500\nHello there.\n\n"
            "2\n01:01:01,250 --> 01:01:02,000\nLater\n\n");
        QCOMPARE(t.toSrt(), expected);
    }

    void testSrtClipsOverlappingLines() {
        SubtitleTimeline t;
        t.addResult(0.0, 25.0, {{1.0, 6.0, QStringLiteral("one")}, {5.0, 8.0, QStringLiteral("two")}});
        QVERIFY(t.toSrt().contains(QStringLiteral("00:00:01,000 --> 00:00:05,000")));
        QVERIFY(t.toSrt().contains(QStringLiteral("00:00:05,000 --> 00:00:08,000")));
    }

    void testSrtTimeFormatting() {
        QCOMPARE(SubtitleTimeline::formatSrtTime(0.0), QStringLiteral("00:00:00,000"));
        QCOMPARE(SubtitleTimeline::formatSrtTime(59.9996), QStringLiteral("00:01:00,000"));
        QCOMPARE(SubtitleTimeline::formatSrtTime(-3.0), QStringLiteral("00:00:00,000"));
    }

    void testSpeechStartSkipsLeadingSilence() {
        const auto pcm = makeAudio(10.0, 2.2, 7.0);
        const SpeechActivity activity(pcm.data(), pcm.size(), kRate);
        const double start = activity.speechStart(0.0, 7.0);
        QVERIFY2(std::abs(start - 2.2) < 0.05, qPrintable(QString::number(start)));
    }

    void testSpeechStartKeepsStartWhenAlreadySpeaking() {
        const auto pcm = makeAudio(10.0, 2.2, 7.0);
        const SpeechActivity activity(pcm.data(), pcm.size(), kRate);
        QCOMPARE(activity.speechStart(3.0, 7.0), 3.0);
    }

    void testSpeechStartSkipsPreviousSentenceTail() {
        // Two sentences: 1.0-4.0 and 5.0-8.0; whisper put the second one's start at 3.0
        auto pcm = makeAudio(10.0, 1.0, 4.0);
        const auto second = makeAudio(10.0, 5.0, 8.0);
        for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = std::max(pcm[i], second[i]);
        const SpeechActivity activity(pcm.data(), pcm.size(), kRate);
        const double start = activity.speechStart(3.0, 8.0);
        QVERIFY2(std::abs(start - 5.0) < 0.05, qPrintable(QString::number(start)));
    }

    void testSpeechStartFallsBackWithoutVoice() {
        const auto pcm = makeAudio(10.0, 20.0, 21.0);
        const SpeechActivity activity(pcm.data(), pcm.size(), kRate);
        QCOMPARE(activity.speechStart(1.0, 5.0), 1.0);
    }

    void testSpeechStartIgnoresOnsetTooCloseToEnd() {
        const auto pcm = makeAudio(10.0, 4.9, 6.0);
        const SpeechActivity activity(pcm.data(), pcm.size(), kRate);
        QCOMPARE(activity.speechStart(1.0, 5.0), 1.0);
    }

    void testModelSelection() {
        using namespace Omnipot::WhisperModels;
        QCOMPARE(autoSelect(2), QStringLiteral("tiny"));
        QCOMPARE(autoSelect(4), QStringLiteral("base"));
        QCOMPARE(autoSelect(16), QStringLiteral("small"));
        QCOMPARE(resolve(QStringLiteral("auto"), 2), QStringLiteral("tiny"));
        QCOMPARE(resolve(QStringLiteral("small"), 2), QStringLiteral("small"));
        QCOMPARE(resolve(QStringLiteral("bogus"), 4), QStringLiteral("base"));
        QCOMPARE(recommendedThreads(1), 1);
        QCOMPARE(recommendedThreads(2), 1);
        QCOMPARE(recommendedThreads(8), 4);
        QCOMPARE(recommendedThreads(32), 4);
    }

    void testModelPaths() {
        using namespace Omnipot::WhisperModels;
        QVERIFY(modelPath(QStringLiteral("tiny")).endsWith(QStringLiteral("/omnipot/models/ggml-tiny.bin")));
        QCOMPARE(downloadUrl(QStringLiteral("base")).toString(),
                 QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"));
        QVERIFY(!isInstalled(QStringLiteral("tiny")));
    }
};

QTEST_MAIN(TestRealtimeSubtitle)
#include "test_realtime_subtitle.moc"
