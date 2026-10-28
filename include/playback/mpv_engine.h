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

    QList<TrackInfo> audioTracks() const override { return m_audioTracks; }
    QList<TrackInfo> subtitleTracks() const override { return m_subTracks; }
    QStringList audioDevices() const override;
    QString currentAudioDevice() const override { return m_audioDevice; }
    QList<ChapterInfo> chapters() const override { return m_chapters; }
    int currentChapter() const override { return m_currentChapter; }

    int brightness() const override { return m_brightness; }
    int contrast() const override { return m_contrast; }
    int saturation() const override { return m_saturation; }
    int hue() const override { return m_hue; }

    bool isEqualizerEnabled() const override { return m_equalizerEnabled; }
    double equalizerBand(int bandIndex) const override;

    QString hwdec() const override { return m_hwdec; }
    int rotation() const override { return m_rotation; }
    bool isHorizontalFlip() const override { return m_hflip; }
    bool isVerticalFlip() const override { return m_vflip; }
    bool isDeinterlacing() const override { return m_deinterlace; }

    int subtitleFontSize() const override { return m_subtitleFontSize; }
    int subtitlePosition() const override { return m_subtitlePosition; }
    bool isAssOverride() const override { return m_assOverride; }
    double audioDelay() const override { return m_audioDelay; }
    double subtitleDelay() const override { return m_subtitleDelay; }

    double loopA() const override { return m_loopA; }
    double loopB() const override { return m_loopB; }

    StreamStats streamStats() const override;

    mpv_handle* rawHandle() const { return m_mpv; }
    mpv_handle* rawMpvHandle() const override { return m_mpv; }

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

    void setAudioTrack(int id) override;
    void setSubtitleTrack(int id) override;
    void cycleAudioTrack() override;
    void cycleSubtitleTrack() override;
    void setAudioDelay(double seconds) override;
    void setSubtitleDelay(double seconds) override;
    void setAudioDevice(const QString &device) override;
    void showOsdText(const QString &text, int timeoutMs = 1500) override;
    bool isNormalizerEnabled() const override { return m_normalizerEnabled; }
    void setNormalizerEnabled(bool enable) override;

    // Milestone 3
    void setHwdec(const QString &mode) override;
    void setBrightness(int value) override;
    void setContrast(int value) override;
    void setSaturation(int value) override;
    void setHue(int value) override;
    void resetColors() override;

    void setEqualizerEnabled(bool enabled) override;
    void setEqualizerBand(int bandIndex, double gainDb) override;
    void setEqualizerPreset(const QString &presetName) override;
    void resetEqualizer() override;

    void setRotation(int degrees) override;
    void rotateClockwise() override;
    void setHorizontalFlip(bool flip) override;
    void setVerticalFlip(bool flip) override;
    void setDeinterlacing(bool enable) override;

    void setSubtitleFontSize(int size) override;
    void setSubtitlePosition(int pos) override;
    void setAssOverride(bool override) override;

    void setChapter(int index) override;
    void nextChapter() override;
    void previousChapter() override;

    void setLoopA(double seconds) override;
    void setLoopB(double seconds) override;
    void clearLoop() override;

    void setWid(int64_t wid);

private Q_SLOTS:
    void processMpvEvents();

private:
    void handleMpvEvent(mpv_event *event);
    void handlePropertyChange(mpv_event_property *prop);
    void updateTrackList();
    void updateChapterList();
    void applyEqualizer();
    void applyAudioFilters();
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
    QList<TrackInfo> m_audioTracks;
    QList<TrackInfo> m_subTracks;
    QList<ChapterInfo> m_chapters;
    int m_currentChapter = 0;

    // Colors
    int m_brightness = 0;
    int m_contrast = 0;
    int m_saturation = 0;
    int m_hue = 0;

    // Equalizer
    bool m_equalizerEnabled = false;
    std::array<double, 10> m_eqGains{};

    // Video Transform & HWDEC
    QString m_hwdec = QStringLiteral("auto-safe");
    int m_rotation = 0;
    bool m_hflip = false;
    bool m_vflip = false;
    bool m_deinterlace = false;

    // Subtitles & Delays
    int m_subtitleFontSize = 55;
    int m_subtitlePosition = 100;
    bool m_assOverride = false;
    double m_audioDelay = 0.0;
    double m_subtitleDelay = 0.0;
    QString m_audioDevice = QStringLiteral("auto");
    bool m_normalizerEnabled = false;

    // Loop
    double m_loopA = -1.0;
    double m_loopB = -1.0;

    QTimer *m_eventTimer = nullptr;
};

} // namespace PotPlayer
