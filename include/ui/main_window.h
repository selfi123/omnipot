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
#include "playlist/playlist_manager.h"
#include "ui/playlist_drawer.h"
#include "ui/control_box.h"
#include "ui/info_overlay.h"
#include "ui/title_bar.h"
#include "config/settings_manager.h"

namespace Omnipot {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(PlaybackEngine *engine, QWidget *parent = nullptr);
    ~MainWindow() override = default;

    PlaylistManager* playlistManager() const { return m_playlistManager; }

    // Adds the paths to the playlist and starts playing the first one added.
    // startSeconds < 0 means "use the remembered resume position".
    void openPaths(const QStringList &paths, double startSeconds = -1.0);

public Q_SLOTS:
    void openMediaFile(const QString &filePath, double startSeconds = -1.0);
    void promptOpenFile();
    void promptOpenUrl();
    void togglePlaylistDrawer();
    void toggleControlBox();
    void toggleInfoOverlay();
    void addBookmarkAtCurrentPosition();
    void captureStillFrame();
    void copyFrameToClipboard();
    void openConsecutiveCaptureDialog();
    void openAudioExtractDialog();
    void openSubtitleSearchDialog();
    void promptLoadLocalSubtitle();
    void openPreferencesDialog();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onStateChanged(PlaybackState state);
    void onPositionChanged(double seconds);
    void onDurationChanged(double seconds);
    void onVolumeChanged(int volume);
    void onMuteChanged(bool muted);
    void onSpeedChanged(double speed);
    void onMetadataChanged(const MediaMetadata &metadata);
    void onErrorOccurred(const QString &error);
    void onTracksChanged();
    void onChaptersChanged();
    void onEofReached();

    void toggleFullscreen();
    void showContextMenu(const QPoint &globalPos);
    void toggleTimeDisplayMode();

private:
    void setupUi();
    void setupContextMenu();
    void rebuildTrackMenus();
    void rebuildChaptersMenu();
    void rebuildBookmarksMenu();
    void rebuildShaderMenu();
    void rebuildCaptureMenu();
    void updateBadges(const MediaMetadata &meta);
    QString formatTime(double seconds) const;
    void toggleStayOnTop(bool enable);
    void updateWindowBorder();
    Qt::Edges calculateResizeEdges(const QPoint &pos) const;
    void updateCursorShape(Qt::Edges edges);
    void saveResumePosition();
    void applyStoredPreferences();
    void applyPlaylistPreferences();

    PlaybackEngine *m_engine = nullptr;
    PlaylistManager *m_playlistManager = nullptr;
    PlaylistDrawer *m_playlistDrawer = nullptr;
    ControlBoxDialog *m_controlBox = nullptr;
    InfoOverlay *m_infoOverlay = nullptr;

    TitleBar *m_titleBar = nullptr;
    QWidget *m_centralContainer = nullptr;
    bool m_isStayOnTop = false;
    QAction *m_stayOnTopAct = nullptr;

    VideoWidget *m_videoWidget = nullptr;
    TimeBar *m_timeBar = nullptr;

    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_playlistBtn = nullptr;
    QPushButton *m_controlBoxBtn = nullptr;

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
    QMenu *m_audioTracksMenu = nullptr;
    QMenu *m_subtitleTracksMenu = nullptr;
    QMenu *m_chaptersMenu = nullptr;
    QMenu *m_bookmarksMenu = nullptr;
    QMenu *m_shadersMenu = nullptr;
    QMenu *m_captureMenu = nullptr;

    class FrameCapture *m_frameCapture = nullptr;
    class RealtimeSubtitleController *m_realtimeSubtitle = nullptr;

    QString m_currentFilePath;
    double m_currentPosition = 0.0;
    double m_currentDuration = 0.0;
    double m_pendingStart = -1.0;
};

} // namespace Omnipot
