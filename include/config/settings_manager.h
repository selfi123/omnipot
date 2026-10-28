#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QByteArray>

namespace PotPlayer {

class SettingsManager : public QObject {
    Q_OBJECT

public:
    static SettingsManager& instance();

    void load();
    void sync();

    // Window Geometry
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geom);

    bool isWindowMaximized() const;
    void setWindowMaximized(bool max);

    // Audio Settings
    int volume() const;
    void setVolume(int vol);

    bool isMuted() const;
    void setMuted(bool mute);

    // Playback Settings
    double speed() const;
    void setSpeed(double spd);

    QString defaultAspectRatio() const;
    void setDefaultAspectRatio(const QString &ratio);

    QString hwdec() const;
    void setHwdec(const QString &mode);

    bool rememberPlaybackPosition() const;
    void setRememberPlaybackPosition(bool enable);

    // Recent Files & Resume History
    QStringList recentFiles() const;
    void addRecentFile(const QString &filePath);
    void clearRecentFiles();

    double resumePosition(const QString &filePath) const;
    void setResumePosition(const QString &filePath, double seconds);

    // Playlist Settings
    int repeatMode() const;
    void setRepeatMode(int mode);

    bool isShuffle() const;
    void setShuffle(bool shuf);

    // Video & Audio Advanced Settings
    QString videoScaler() const;
    void setVideoScaler(const QString &scaler);

    bool isAudioNormalization() const;
    void setAudioNormalization(bool enable);

    QString audioChannels() const;
    void setAudioChannels(const QString &channels);

    // Preset & Defaults Management
    void resetToDefaults();
    bool exportSettings(const QString &filePath) const;
    bool importSettings(const QString &filePath);

    // Bookmarks System
    struct Bookmark {
        QString filePath;
        double time = 0.0;
        QString title;
    };

    QList<Bookmark> bookmarks(const QString &filePath) const;
    void addBookmark(const QString &filePath, double seconds, const QString &title = QString());
    void removeBookmark(const QString &filePath, int index);
    void clearBookmarks(const QString &filePath);

private:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager() override = default;

    std::unique_ptr<QSettings> m_settings;
};

} // namespace PotPlayer
