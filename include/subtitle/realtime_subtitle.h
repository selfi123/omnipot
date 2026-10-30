#pragma once

#include <QObject>
#include <QString>
#include "playback/playback_engine.h"

class QWidget;

namespace Omnipot {

class RealtimeSubtitleWorker;

enum class RealtimeSubtitleMode {
    Off = 0,
    Original = 1,           // subtitles in the spoken language
    TranslateToEnglish = 2,
};

// "Realtime subtitle": generates subtitles from the media's speech while it plays.
// Owns the background worker, downloads the speech model on first use, and feeds
// the result to mpv as a normal external subtitle track.
class RealtimeSubtitleController : public QObject {
    Q_OBJECT

public:
    RealtimeSubtitleController(PlaybackEngine *engine, QWidget *dialogParent);
    ~RealtimeSubtitleController() override;

    // False when Omnipot was built without whisper.cpp
    static bool isAvailable();

    RealtimeSubtitleMode mode() const { return m_mode; }
    // Persists the mode; turning it on may ask to download the speech model
    void setMode(RealtimeSubtitleMode mode);

    // Called just before a new file starts loading
    void mediaOpened(const QString &url);
    // Re-reads model and language after the Preferences dialog closes
    void reloadSettings();

Q_SIGNALS:
    void modeChanged(Omnipot::RealtimeSubtitleMode mode);

private:
    void onStateChanged(PlaybackState state);
    void onTracksChanged();
    void onSubtitlesUpdated(const QString &srt);
    // holdPlayback pauses the player until the first window is done, so no opening lines are skipped
    void startWorker(bool holdPlayback = false);
    void stopWorker();
    void endHold();
    void removeTrack();
    bool ensureModel(const QString &modelId);
    bool downloadModel(const QString &modelId);
    int currentAudioStreamIndex() const;
    int ownTrackId() const;
    QString modelId() const;

    PlaybackEngine *m_engine = nullptr;
    QWidget *m_dialogParent = nullptr;
    RealtimeSubtitleMode m_mode = RealtimeSubtitleMode::Off;

    QString m_mediaUrl;
    bool m_mediaReady = false;      // file is loaded and its tracks are known
    QString m_unsupportedNotice;    // reason the current media can't be used, shown once

    RealtimeSubtitleWorker *m_worker = nullptr;
    int m_generation = 0;
    bool m_holdingPlayback = false;
    int m_workerAudioStream = -1;
    QString m_srtPath;
    bool m_trackAdded = false;
};

} // namespace Omnipot
