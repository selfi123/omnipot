#include "subtitle/realtime_subtitle_worker.h"
#include "subtitle/speech_activity.h"
#include "subtitle/subtitle_timeline.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QtEndian>
#include <ggml-backend.h>
#include <whisper.h>
#include <mutex>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace Omnipot {

static constexpr int kSampleRate = WHISPER_SAMPLE_RATE;
// Whisper always encodes 30 s of audio, so shorter windows cost the same and give it less context
static constexpr double kWindowSeconds = 25.0;
// After opening or seeking nothing is ready at the playhead, so start with a short window
// (encoded with a matching smaller audio context) to get the first line on screen quickly
static constexpr double kQuickWindowSeconds = 8.0;
// How far ahead of the playhead to transcribe before going idle
static constexpr double kLookaheadSeconds = 60.0;
// Whisper's segment start times can be late by a few hundred ms; back up a little so no words are lost
static constexpr double kRewindSeconds = 0.6;
// A last segment ending this close to the window's end was probably cut mid-sentence
static constexpr double kCutEdgeSeconds = 0.5;
// Processing seconds per second of audio, assumed until the first window has been measured
static constexpr double kInitialCostRatio = 0.6;
// Extra head start when jumping ahead of the playhead, covering ffmpeg start-up and timing noise
static constexpr double kLeadMarginSeconds = 1.0;
// Nice value for transcription: below playback, but unlike SCHED_IDLE it still gets CPU on a busy machine
static constexpr int kNiceLevel = 10;

// "[Music]", "(laughs)", "♪♪" and similar are not speech worth showing
static bool isNonSpeech(const QString &text) {
    static const QRegularExpression bracketed(QStringLiteral(R"(\[[^\]]*\]|\([^)]*\)|\*[^*]*\*)"));
    static const QRegularExpression noLetters(QStringLiteral(R"(^[\s\p{P}\p{S}]*$)"));
    QString stripped = text;
    stripped.remove(bracketed);
    return noLetters.match(stripped).hasMatch();
}

RealtimeSubtitleWorker::RealtimeSubtitleWorker(const Config &config, QObject *parent)
    : QThread(parent)
    , m_config(config)
    , m_position(config.startPosition)
    , m_duration(config.duration)
{
}

RealtimeSubtitleWorker::~RealtimeSubtitleWorker() {
    requestStop();
    wait();
}

void RealtimeSubtitleWorker::setPosition(double seconds) {
    QMutexLocker lock(&m_mutex);
    m_position = seconds;
    m_wake.wakeAll();
}

void RealtimeSubtitleWorker::setDuration(double seconds) {
    QMutexLocker lock(&m_mutex);
    m_duration = seconds;
    m_wake.wakeAll();
}

void RealtimeSubtitleWorker::requestStop() {
    QMutexLocker lock(&m_mutex);
    m_stop = true;
    m_wake.wakeAll();
}

bool RealtimeSubtitleWorker::stopRequested() {
    QMutexLocker lock(&m_mutex);
    return m_stop;
}

bool RealtimeSubtitleWorker::extractAudio(double start, double length, std::vector<float> &pcm, QString &error) {
    QStringList args = {
        QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), QString::number(start, 'f', 3),
        QStringLiteral("-t"), QString::number(length, 'f', 3),
        QStringLiteral("-i"), m_config.mediaUrl,
        QStringLiteral("-map"), m_config.audioStreamIndex >= 0 ? QStringLiteral("0:%1").arg(m_config.audioStreamIndex) : QStringLiteral("0:a:0"),
        QStringLiteral("-vn"), QStringLiteral("-sn"), QStringLiteral("-dn"),
        QStringLiteral("-ac"), QStringLiteral("1"), QStringLiteral("-ar"), QString::number(kSampleRate),
        QStringLiteral("-f"), QStringLiteral("s16le"), QStringLiteral("pipe:1"),
    };

    // Started from this low-priority thread, so ffmpeg inherits its nice value
    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"), args);
    if (!ffmpeg.waitForStarted(5000)) {
        error = QStringLiteral("ffmpeg is not installed");
        return false;
    }

    QByteArray raw;
    while (ffmpeg.state() != QProcess::NotRunning) {
        ffmpeg.waitForFinished(100);
        raw += ffmpeg.readAllStandardOutput();
        if (stopRequested()) {
            ffmpeg.kill();
            ffmpeg.waitForFinished(1000);
            return false;
        }
    }
    raw += ffmpeg.readAllStandardOutput();

    if (ffmpeg.exitCode() != 0 && raw.isEmpty()) {
        const QStringList lines = QString::fromUtf8(ffmpeg.readAllStandardError()).trimmed().split(QLatin1Char('\n'));
        error = lines.isEmpty() || lines.last().isEmpty() ? QStringLiteral("could not read the audio") : lines.last();
        return false;
    }

    const auto *samples = reinterpret_cast<const qint16 *>(raw.constData());
    const size_t count = static_cast<size_t>(raw.size()) / sizeof(qint16);
    pcm.resize(count);
    for (size_t i = 0; i < count; ++i) {
        pcm[i] = static_cast<float>(qFromLittleEndian(samples[i])) / 32768.0f;
    }
    return true;
}

void RealtimeSubtitleWorker::run() {
    // whisper.cpp logs every load step to stderr; keep the terminal readable
    whisper_log_set([](enum ggml_log_level, const char *, void *) {}, nullptr);

    // Linux nice values are per thread; ffmpeg processes started from here inherit it
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), kNiceLevel);

    // Distro builds of ggml (Debian, Kali, Fedora) ship the CPU code as plug-ins that
    // must be loaded before any model; without this whisper aborts the whole player
    static std::once_flag backendsLoaded;
    std::call_once(backendsLoaded, [] { ggml_backend_load_all(); });

    whisper_context_params contextParams = whisper_context_default_params();
    contextParams.use_gpu = false;
    whisper_context *ctx = whisper_init_from_file_with_params(m_config.modelPath.toUtf8().constData(), contextParams);
    if (!ctx) {
        Q_EMIT failed(QStringLiteral("could not load the speech model"));
        return;
    }
    Q_EMIT modelReady();

    SubtitleTimeline timeline;
    double quickCost = kInitialCostRatio;
    double fullCost = kInitialCostRatio;
    double lastWindowEnd = -1.0;
    while (!stopRequested()) {
        double position = 0.0;
        double duration = 0.0;
        {
            QMutexLocker lock(&m_mutex);
            position = m_position;
            duration = m_duration;
        }

        const bool covered = timeline.isCovered(position);
        // Just opened or seeked: nothing done near the playhead, so use a short window for a fast first line.
        // Merely falling behind keeps full windows, which are cheaper per second of audio.
        const bool behind = lastWindowEnd >= 0.0 && lastWindowEnd >= position - kWindowSeconds
                            && lastWindowEnd <= position + kLeadMarginSeconds;
        const bool quick = !covered && !behind;
        const double windowLength = quick ? kQuickWindowSeconds : kWindowSeconds;
        // Audio the playhead reaches before this window is processed would get its subtitles too late,
        // so start that far ahead. The skipped part has no subtitles, but everything after is on time.
        double from = position;
        // While the player waits for the first window there is no need to skip ahead
        const bool held = m_config.playbackHeld && lastWindowEnd < 0.0;
        if (!covered && !held) {
            from += (quick ? quickCost : fullCost) * windowLength + kLeadMarginSeconds;
        }
        const auto window = timeline.nextWindow(from, duration, kLookaheadSeconds, windowLength);
        if (!window) {
            QMutexLocker lock(&m_mutex);
            if (!m_stop) m_wake.wait(&m_mutex, 500);
            continue;
        }

        const double requested = window->end - window->start;
        qInfo() << "[RealtimeSubtitle] Processing window:" << window->start << "->" << window->end;
        QElapsedTimer cost;
        cost.start();
        std::vector<float> pcm;
        QString error;
        if (!extractAudio(window->start, requested, pcm, error)) {
            if (!stopRequested()) Q_EMIT failed(error);
            break;
        }

        const double got = static_cast<double>(pcm.size()) / kSampleRate;
        // Short read: the media ends inside this window, or the window was clipped by an existing range
        const bool isLastPiece = got < requested - 0.5 || requested < windowLength - 0.01;
        if (got < 1.0) {
            timeline.addResult(window->start, window->end, {});
            lastWindowEnd = window->end;
            Q_EMIT windowProcessed();
            continue;
        }

        const QByteArray language = m_config.language != QStringLiteral("auto")
            ? m_config.language.toUtf8()
            : (m_detectedLanguage.isEmpty() ? QByteArrayLiteral("auto") : m_detectedLanguage.toUtf8());

        whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        params.n_threads = m_config.threads;
        params.translate = m_config.translate;
        params.language = language.constData();
        params.no_context = true;
        params.suppress_blank = true;
        params.suppress_nst = true;
        if (quick) {
            // Encode ~15 s instead of the full 30 s (1500). Measured on a 2-core CPU: first line in
            // ~4 s instead of ~10 s. Going tighter, near the window length, makes whisper loop and run slower.
            params.audio_ctx = 768;
        }
        params.print_progress = false;
        params.print_realtime = false;
        params.print_special = false;
        params.print_timestamps = false;
        // Lets requestStop() interrupt a running transcription instead of blocking the UI until it ends
        params.abort_callback = [](void *worker) { return static_cast<RealtimeSubtitleWorker *>(worker)->stopRequested(); };
        params.abort_callback_user_data = this;

        if (whisper_full(ctx, params, pcm.data(), static_cast<int>(pcm.size())) != 0) {
            if (!stopRequested()) Q_EMIT failed(QStringLiteral("speech recognition failed"));
            break;
        }

        // Running average, so the head start adapts to how busy the machine is
        double &costRatio = quick ? quickCost : fullCost;
        costRatio = 0.5 * costRatio + 0.5 * (static_cast<double>(cost.elapsed()) / 1000.0 / got);

        const int count = whisper_full_n_segments(ctx);
        int keep = count;
        double consumed = isLastPiece ? requested : got;
        if (!isLastPiece && count >= 1) {
            // A sentence running into the window's end is cut; redo it from its start in the next window
            const double lastStart = static_cast<double>(whisper_full_get_segment_t0(ctx, count - 1)) / 100.0;
            const double lastEnd = static_cast<double>(whisper_full_get_segment_t1(ctx, count - 1)) / 100.0;
            if (lastEnd > got - kCutEdgeSeconds && lastStart - kRewindSeconds > 1.0) {
                keep = count - 1;
                consumed = lastStart - kRewindSeconds;
            }
        }

        // Whisper often starts a segment in the silence before the words; move it to where the voice begins
        const SpeechActivity activity(pcm.data(), pcm.size(), kSampleRate);
        QList<SubtitleSegment> segments;
        for (int i = 0; i < keep; ++i) {
            const QString text = QString::fromUtf8(whisper_full_get_segment_text(ctx, i)).trimmed();
            if (text.isEmpty() || isNonSpeech(text)) continue;
            const double t0 = static_cast<double>(whisper_full_get_segment_t0(ctx, i)) / 100.0;
            const double t1 = static_cast<double>(whisper_full_get_segment_t1(ctx, i)) / 100.0;
            SubtitleSegment seg;
            seg.start = window->start + activity.speechStart(t0, t1);
            seg.end = window->start + t1;
            seg.text = text;
            segments.append(seg);
        }

        // Lock the language after the first real speech so it doesn't flip between windows
        if (m_config.language == QStringLiteral("auto") && m_detectedLanguage.isEmpty() && !segments.isEmpty()) {
            m_detectedLanguage = QString::fromUtf8(whisper_lang_str(whisper_full_lang_id(ctx)));
        }

        timeline.addResult(window->start, window->start + consumed, segments);
        lastWindowEnd = window->start + consumed;
        qInfo() << "[RealtimeSubtitle] Window done:" << window->start << "->" << (window->start + consumed)
                << "segments:" << segments.size() << "in" << cost.elapsed() << "ms";
        if (!segments.isEmpty()) {
            Q_EMIT subtitlesUpdated(timeline.toSrt());
        }
        Q_EMIT windowProcessed();
    }

    whisper_free(ctx);
}

} // namespace Omnipot
