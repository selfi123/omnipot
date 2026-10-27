#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace PotPlayer {

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
    int audioChannels = 2;
    int audioSampleRate = 44100;
};

class PlaybackEngine : public QObject {
    Q_OBJECT

public:
    explicit PlaybackEngine(QObject *parent = nullptr) : QObject(parent) {}
    ~PlaybackEngine() override = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual PlaybackState state() const = 0;
    virtual double position() const = 0;
    virtual double duration() const = 0;
    virtual int volume() const = 0;
    virtual bool isMuted() const = 0;
    virtual double speed() const = 0;
    virtual const MediaMetadata& metadata() const = 0;

public Q_SLOTS:
    virtual void open(const QString &url) = 0;
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

Q_SIGNALS:
    void stateChanged(PotPlayer::PlaybackState state);
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void volumeChanged(int volume);
    void muteChanged(bool muted);
    void speedChanged(double speed);
    void metadataChanged(const PotPlayer::MediaMetadata &metadata);
    void errorOccurred(const QString &errorMessage);
    void eofReached();
};

} // namespace PotPlayer
