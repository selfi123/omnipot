#pragma once

#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include <vector>

struct whisper_context;

namespace Omnipot {

// Background thread that reads audio ahead of the playhead with ffmpeg and turns
// it into timed text with whisper.cpp. Runs at low CPU priority so video
// decoding always wins; if it falls behind, it skips ahead of the playhead so
// the subtitles it does make are on time.
class RealtimeSubtitleWorker : public QThread {
    Q_OBJECT

public:
    struct Config {
        QString mediaUrl;
        QString modelPath;
        QString language = QStringLiteral("auto"); // whisper code or "auto"
        bool translate = false;                    // translate speech to English
        int threads = 1;
        int audioStreamIndex = -1;                 // ffmpeg stream index, -1 = first audio stream
        double duration = 0.0;
        double startPosition = 0.0;
        bool playbackHeld = false;                 // player waits for the first window, so no head start is needed
    };

    explicit RealtimeSubtitleWorker(const Config &config, QObject *parent = nullptr);
    ~RealtimeSubtitleWorker() override;

    void setPosition(double seconds);
    void setDuration(double seconds);
    void requestStop();

Q_SIGNALS:
    void modelReady();
    void windowProcessed();
    void subtitlesUpdated(const QString &srt);
    void failed(const QString &message);

protected:
    void run() override;

private:
    bool stopRequested();
    bool extractAudio(double start, double length, std::vector<float> &pcm, QString &error);

    const Config m_config;
    QString m_detectedLanguage;

    QMutex m_mutex;
    QWaitCondition m_wake;
    double m_position = 0.0;
    double m_duration = 0.0;
    bool m_stop = false;
};

} // namespace Omnipot
