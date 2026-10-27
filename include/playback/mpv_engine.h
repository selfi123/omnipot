#pragma once

#include "playback/playback_engine.h"
#include <mpv/client.h>
#include <QTimer>
#include <memory>

namespace PotPlayer {

class MpvEngine : public PlaybackEngine {
    Q_OBJECT

public:
    explicit MpvEngine(QObject *parent = nullptr);
    ~MpvEngine() override;

    bool initialize() override;
    void shutdown() override;

    PlaybackState state() const override { return m_state; }
    double position() const override { return m_position; }
    double duration() const override { return m_duration; }
    int volume() const override { return m_volume; }
    bool isMuted() const override { return m_isMuted; }
    double speed() const override { return m_speed; }
    const MediaMetadata& metadata() const override { return m_metadata; }

    mpv_handle* rawHandle() const { return m_mpv; }

public Q_SLOTS:
    void open(const QString &url) override;
    void play() override;
    void pause() override;
    void stop() override;
    void togglePlayPause() override;

    void seek(double seconds, bool exact = true) override;
    void seekRelative(double offsetSeconds) override;
    void frameStep() override;
    void frameBackStep() override;

    void setVolume(int volume) override;
    void setMuted(bool muted) override;
    void toggleMute() override;

    void setSpeed(double speed) override;
    void setAspectRatio(const QString &aspectRatio) override;

    void setWid(int64_t wid);

private Q_SLOTS:
    void processMpvEvents();

private:
    void handleMpvEvent(mpv_event *event);
    void handlePropertyChange(mpv_event_property *prop);
    void setState(PlaybackState state);

    static void onMpvWakeup(void *ctx);

    mpv_handle *m_mpv = nullptr;
    PlaybackState m_state = PlaybackState::Idle;
    double m_position = 0.0;
    double m_duration = 0.0;
    int m_volume = 100;
    bool m_isMuted = false;
    double m_speed = 1.0;
    MediaMetadata m_metadata;

    QTimer *m_eventTimer = nullptr;
};

} // namespace PotPlayer
