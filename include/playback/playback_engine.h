#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

struct mpv_handle;

namespace Omnipot {

enum class PlaybackState {
    Idle,
    Opening,
    Playing,
    Paused,
    Stopped,
    Error
};

struct MediaMetadata {
    QString title;
    QString filename;
    QString format;
    QString videoCodec;
    QString audioCodec;
    int videoWidth = 0;
    int videoHeight = 0;
    double fps = 0.0;
    qint64 bitrate = 0;
    double duration = 0.0;
    bool isHardwareDecoded = false;
    bool isHdr = false;
    int audioChannels = 0; // 0 = unknown / no audio
    int audioSampleRate = 44100;
};

struct TrackInfo {
    int id = -1;
    QString type; // "audio", "sub", "video"
    QString title;
    QString language;
    QString codec;
    bool isSelected = false;
    bool isDefault = false;
};

struct ChapterInfo {
    int id = 0;
    QString title;
    double time = 0.0;
};

struct StreamStats {
    QString videoCodec;
    QString audioCodec;
    int videoWidth = 0;
    int videoHeight = 0;
    double fps = 0.0;
    QString hwdec;
    qint64 videoBitrate = 0;
    qint64 audioBitrate = 0;
    int audioSampleRate = 0;
    int audioChannels = 0;
    qint64 droppedFrames = 0;
    double avSyncDiff = 0.0;
};

class PlaybackEngine : public QObject {
    Q_OBJECT

public:
    explicit PlaybackEngine(QObject *parent = nullptr) : QObject(parent) {}
    ~PlaybackEngine() override;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual PlaybackState state() const = 0;
    virtual double position() const = 0;
    virtual double duration() const = 0;
    virtual int volume() const = 0;
    virtual bool isMuted() const = 0;
    virtual double speed() const = 0;
    virtual const MediaMetadata& metadata() const = 0;

    virtual QList<TrackInfo> audioTracks() const = 0;
    virtual QList<TrackInfo> subtitleTracks() const = 0;
    virtual QStringList audioDevices() const { return {}; }
    virtual QString currentAudioDevice() const { return QStringLiteral("auto"); }
    virtual QList<ChapterInfo> chapters() const = 0;
    virtual int currentChapter() const = 0;

    // ProcAmp / Colors (-100 to 100, default 0)
    virtual int brightness() const = 0;
    virtual int contrast() const = 0;
    virtual int saturation() const = 0;
    virtual int hue() const = 0;

    // 10-band Equalizer (-12.0dB to +12.0dB, default 0.0)
    virtual bool isEqualizerEnabled() const = 0;
    virtual double equalizerBand(int bandIndex) const = 0;

    // Video Transformation & HWDEC
    virtual QString hwdec() const = 0;
    virtual int rotation() const = 0;
    virtual bool isHorizontalFlip() const = 0;
    virtual bool isVerticalFlip() const = 0;
    virtual bool isDeinterlacing() const = 0;

    // Subtitle Styling & Delays
    virtual int subtitleFontSize() const = 0;
    virtual int subtitlePosition() const = 0;
    virtual bool isAssOverride() const = 0;
    virtual double audioDelay() const = 0;
    virtual double subtitleDelay() const = 0;

    // A-B Looping
    virtual double loopA() const = 0;
    virtual double loopB() const = 0;

    // Diagnostics / Stream Stats
    virtual StreamStats streamStats() const = 0;

    virtual mpv_handle* rawMpvHandle() const { return nullptr; }

public Q_SLOTS:
    virtual void open(const QString &url, double startSeconds = 0.0) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void togglePlayPause() = 0;

    virtual void seek(double seconds, bool exact = true) = 0;
    virtual void seekRelative(double offsetSeconds) = 0;
    virtual void frameStep() = 0;
    virtual void frameBackStep() = 0;

    virtual void setVolume(int volume) = 0;
    virtual void setMuted(bool muted) = 0;
    virtual void toggleMute() = 0;

    virtual void setSpeed(double speed) = 0;
    virtual void setAspectRatio(const QString &aspectRatio) = 0;

    virtual void setAudioTrack(int id) = 0;
    virtual void setSubtitleTrack(int id) = 0;
    virtual void cycleAudioTrack() = 0;
    virtual void cycleSubtitleTrack() = 0;
    virtual void setAudioDelay(double seconds) = 0;
    virtual void setSubtitleDelay(double seconds) = 0;
    virtual void setAudioDevice(const QString &device) { Q_UNUSED(device); }
    virtual void showOsdText(const QString &text, int timeoutMs = 1500) { Q_UNUSED(text); Q_UNUSED(timeoutMs); }
    virtual bool isNormalizerEnabled() const { return false; }
    virtual void setNormalizerEnabled(bool enable) { Q_UNUSED(enable); }

    // Milestone 3 Slots
    virtual void setHwdec(const QString &mode) = 0;
    virtual void setBrightness(int value) = 0;
    virtual void setContrast(int value) = 0;
    virtual void setSaturation(int value) = 0;
    virtual void setHue(int value) = 0;
    virtual void resetColors() = 0;

    virtual void setEqualizerEnabled(bool enabled) = 0;
    virtual void setEqualizerBand(int bandIndex, double gainDb) = 0;
    virtual void setEqualizerPreset(const QString &presetName) = 0;
    virtual void resetEqualizer() = 0;

    virtual void setRotation(int degrees) = 0;
    virtual void rotateClockwise() = 0;
    virtual void setHorizontalFlip(bool flip) = 0;
    virtual void setVerticalFlip(bool flip) = 0;
    virtual void setDeinterlacing(bool enable) = 0;

    virtual void setSubtitleFontSize(int size) = 0;
    virtual void setSubtitlePosition(int pos) = 0;
    virtual void setAssOverride(bool override) = 0;

    virtual void setChapter(int index) = 0;
    virtual void nextChapter() = 0;
    virtual void previousChapter() = 0;

    virtual void setLoopA(double seconds) = 0;
    virtual void setLoopB(double seconds) = 0;
    virtual void clearLoop() = 0;

Q_SIGNALS:
    void stateChanged(Omnipot::PlaybackState state);
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void volumeChanged(int volume);
    void muteChanged(bool muted);
    void speedChanged(double speed);
    void metadataChanged(const Omnipot::MediaMetadata &metadata);
    void tracksChanged();
    void chaptersChanged();
    void colorsChanged();
    void equalizerChanged();
    void errorOccurred(const QString &errorMessage);
    void eofReached();
};

} // namespace Omnipot
