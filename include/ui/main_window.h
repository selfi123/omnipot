#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QMenu>
#include <memory>
#include "playback/playback_engine.h"
#include "video/video_widget.h"
#include "ui/time_bar.h"

namespace PotPlayer {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(PlaybackEngine *engine, QWidget *parent = nullptr);
    ~MainWindow() override = default;

public Q_SLOTS:
    void openMediaFile(const QString &filePath);
    void promptOpenFile();
    void promptOpenUrl();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private Q_SLOTS:
    void onStateChanged(PlaybackState state);
    void onPositionChanged(double seconds);
    void onDurationChanged(double seconds);
    void onVolumeChanged(int volume);
    void onMuteChanged(bool muted);
    void onSpeedChanged(double speed);
    void onMetadataChanged(const MediaMetadata &metadata);
    void onErrorOccurred(const QString &error);

    void toggleFullscreen();
    void showContextMenu(const QPoint &globalPos);
    void toggleTimeDisplayMode();

private:
    void setupUi();
    void setupContextMenu();
    void updateBadges(const MediaMetadata &meta);
    QString formatTime(double seconds) const;

    PlaybackEngine *m_engine = nullptr;

    VideoWidget *m_videoWidget = nullptr;
    TimeBar *m_timeBar = nullptr;

    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_openBtn = nullptr;

    QLabel *m_timeLabel = nullptr;
    bool m_showRemainingTime = false;

    QLabel *m_hwSwBadge = nullptr;
    QLabel *m_hdrBadge = nullptr;
    QLabel *m_videoCodecBadge = nullptr;
    QLabel *m_audioCodecBadge = nullptr;
    QLabel *m_channelsBadge = nullptr;

    QPushButton *m_muteBtn = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QPushButton *m_fullscreenBtn = nullptr;

    QMenu *m_contextMenu = nullptr;

    double m_currentPosition = 0.0;
    double m_currentDuration = 0.0;
};

} // namespace PotPlayer
