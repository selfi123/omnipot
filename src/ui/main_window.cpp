#include "ui/main_window.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QWindow>
#include <QStyle>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QToolTip>
#include <QFontMetrics>
#include <QShortcut>
#include <QSignalBlocker>
#include <QCursor>
#include <QActionGroup>
#include <algorithm>
#include <utility>
#include "video/frame_capture.h"
#include "video/shader_manager.h"
#include "ui/consecutive_capture_dialog.h"
#include "ui/audio_extract_dialog.h"
#include "ui/subtitle_search_dialog.h"
#include "ui/open_url_dialog.h"
#include "ui/preferences_dialog.h"
#include "ui/omni_icons.h"
#include "subtitle/realtime_subtitle.h"

namespace Omnipot {

MainWindow::MainWindow(PlaybackEngine *engine, QWidget *parent)
    : QMainWindow(parent)
    , m_engine(engine)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    setMouseTracking(true);
    setWindowTitle(QStringLiteral("Omnipot"));
    resize(1080, 640);
    setMinimumSize(480, 320);
    setAcceptDrops(true);

    m_playlistManager = new PlaylistManager(this);
    m_frameCapture = new FrameCapture(this);
    m_realtimeSubtitle = new RealtimeSubtitleController(m_engine, this);
    ShaderManager::instance().initialize();

    setupUi();
    setupContextMenu();

    // Connect Engine Signals
    connect(m_engine, &PlaybackEngine::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_engine, &PlaybackEngine::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_engine, &PlaybackEngine::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_engine, &PlaybackEngine::volumeChanged, this, &MainWindow::onVolumeChanged);
    connect(m_engine, &PlaybackEngine::muteChanged, this, &MainWindow::onMuteChanged);
    connect(m_engine, &PlaybackEngine::speedChanged, this, &MainWindow::onSpeedChanged);
    connect(m_engine, &PlaybackEngine::metadataChanged, this, &MainWindow::onMetadataChanged);
    connect(m_engine, &PlaybackEngine::tracksChanged, this, &MainWindow::onTracksChanged);
    connect(m_engine, &PlaybackEngine::chaptersChanged, this, &MainWindow::onChaptersChanged);
    connect(m_engine, &PlaybackEngine::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_engine, &PlaybackEngine::eofReached, this, &MainWindow::onEofReached);

    // Connect Playlist Signals
    connect(m_playlistManager, &PlaylistManager::currentItemChanged, this, [this](const PlaylistItem &item) {
        if (item.isValid()) {
            double start = std::exchange(m_pendingStart, -1.0);
            if (start < 0.0 && item.startOffset > 0.0) {
                start = item.startOffset;
            }
            openMediaFile(item.filePath, start);
        }
    });
    connect(m_playlistManager, &PlaylistManager::playbackModeChanged, this, [this]() {
        auto &settings = SettingsManager::instance();
        settings.setRepeatMode(static_cast<int>(m_playlistManager->repeatMode()));
        settings.setShuffle(m_playlistManager->isShuffle());
    });

    // Tab would otherwise be swallowed by focus navigation before keyPressEvent
    auto *infoShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    connect(infoShortcut, &QShortcut::activated, this, &MainWindow::toggleInfoOverlay);

    // Restore persistent settings
    auto &settings = SettingsManager::instance();
    QByteArray geom = settings.windowGeometry();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
    if (settings.isWindowMaximized()) {
        showMaximized();
    }
    m_engine->setVolume(settings.volume());
    m_engine->setMuted(settings.isMuted());
    onVolumeChanged(settings.volume());
    onMuteChanged(settings.isMuted());
    applyStoredPreferences();
}

void MainWindow::applyStoredPreferences() {
    auto &settings = SettingsManager::instance();
    m_engine->setHwdec(settings.hwdec());
    m_engine->setSpeed(settings.speed());
    if (mpv_handle *mpv = m_engine->rawMpvHandle()) {
        mpv_set_property_string(mpv, "scale", settings.videoScaler().toUtf8().constData());
        mpv_set_property_string(mpv, "audio-channels", settings.audioChannels().toUtf8().constData());
    }
    applyPlaylistPreferences();
}

void MainWindow::applyPlaylistPreferences() {
    auto &settings = SettingsManager::instance();
    m_playlistManager->setRepeatMode(static_cast<RepeatMode>(std::clamp(settings.repeatMode(), 0, 2)));
    m_playlistManager->setShuffle(settings.isShuffle());
}

void MainWindow::saveResumePosition() {
    if (m_currentFilePath.isEmpty()) return;
    auto &settings = SettingsManager::instance();
    // A finished (or nearly finished) file should start from the beginning next time
    if (m_currentDuration > 0.0 && m_currentPosition > m_currentDuration - 10.0) {
        settings.setResumePosition(m_currentFilePath, 0.0);
    } else if (m_currentPosition > 5.0) {
        settings.setResumePosition(m_currentFilePath, m_currentPosition);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    auto &settings = SettingsManager::instance();
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowMaximized(isMaximized());
    settings.setVolume(m_engine->volume());
    settings.setMuted(m_engine->isMuted());
    saveResumePosition();
    settings.sync();

    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi() {
    m_centralContainer = new QWidget(this);
    m_centralContainer->setObjectName(QStringLiteral("centralContainer"));
    m_centralContainer->setMouseTracking(true);
    updateWindowBorder();

    // Ensure all tooltips across the application have high-contrast, bright white text with amber border
    qApp->setStyleSheet(QStringLiteral(
        "QToolTip { "
        "   background-color: #242424; "
        "   color: #FFFFFF; "
        "   border: 1px solid #F0A020; "
        "   border-radius: 4px; "
        "   padding: 4px 8px; "
        "   font-family: 'Segoe UI', 'Ubuntu', sans-serif; "
        "   font-size: 11px; "
        "   font-weight: 500; "
        "}"
    ));
    QPalette tipPal = QToolTip::palette();
    tipPal.setColor(QPalette::All, QPalette::ToolTipBase, QColor(0x24, 0x24, 0x24));
    tipPal.setColor(QPalette::All, QPalette::ToolTipText, QColor(0xff, 0xff, 0xff));
    tipPal.setColor(QPalette::All, QPalette::Window, QColor(0x24, 0x24, 0x24));
    tipPal.setColor(QPalette::All, QPalette::WindowText, QColor(0xff, 0xff, 0xff));
    QToolTip::setPalette(tipPal);

    auto *rootLayout = new QVBoxLayout(m_centralContainer);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 0. Top Slim Custom TitleBar
    m_titleBar = new TitleBar(m_centralContainer);
    connect(m_titleBar, &TitleBar::menuRequested, this, &MainWindow::showContextMenu);
    connect(m_titleBar, &TitleBar::pinToggled, this, &MainWindow::toggleStayOnTop);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRequested, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::doubleClicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });

    for (auto *w : {static_cast<QWidget*>(m_titleBar->menuButton()),
                    static_cast<QWidget*>(m_titleBar->pinButton()),
                    static_cast<QWidget*>(m_titleBar->minimizeButton()),
                    static_cast<QWidget*>(m_titleBar->maximizeButton()),
                    static_cast<QWidget*>(m_titleBar->closeButton())}) {
        w->installEventFilter(this);
    }
    rootLayout->addWidget(m_titleBar);

    // 1. Center Area: Video Canvas + Side Playlist Drawer
    auto *contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_videoWidget = new VideoWidget(m_centralContainer);
    m_videoWidget->setFocusPolicy(Qt::StrongFocus);
    m_videoWidget->setMpvHandle(m_engine->rawMpvHandle());
    connect(m_videoWidget, &VideoWidget::doubleClicked, m_engine, &PlaybackEngine::togglePlayPause);
    connect(m_videoWidget, &VideoWidget::middleClicked, this, &MainWindow::toggleFullscreen);
    connect(m_videoWidget, &VideoWidget::rightClicked, this, &MainWindow::showContextMenu);
    connect(m_videoWidget, &VideoWidget::wheelScrolled, this, [this](int delta) {
        int step = (delta > 0) ? 5 : -5;
        int newVol = std::clamp(m_engine->volume() + step, 0, 200);
        m_engine->setVolume(newVol);
        m_engine->showOsdText(QStringLiteral("Volume: %1%").arg(newVol));
    });
    contentLayout->addWidget(m_videoWidget, 1);

    m_infoOverlay = new InfoOverlay(m_engine, m_videoWidget);
    m_controlBox = new ControlBoxDialog(m_engine, this);

    m_playlistDrawer = new PlaylistDrawer(m_playlistManager, m_centralContainer);
    m_playlistDrawer->hide(); // Hidden by default, toggled with F8
    connect(m_playlistDrawer, &PlaylistDrawer::closeRequested, this, &MainWindow::togglePlaylistDrawer);
    contentLayout->addWidget(m_playlistDrawer);

    rootLayout->addLayout(contentLayout, 1);

    // 2. Control Bar Container
    auto *controlBar = new QWidget(m_centralContainer);
    controlBar->setObjectName(QStringLiteral("controlBar"));
    controlBar->setFixedHeight(58);

    auto *controlBarLayout = new QVBoxLayout(controlBar);
    controlBarLayout->setContentsMargins(8, 2, 8, 4);
    controlBarLayout->setSpacing(2);

    // Row A: TimeBar
    m_timeBar = new TimeBar(controlBar);
    connect(m_timeBar, &TimeBar::seekRequested, m_engine, [this](double sec) {
        m_engine->seek(sec, true);
    });
    controlBarLayout->addWidget(m_timeBar);

    // Row B: Controls & Badges
    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(6);

    m_playPauseBtn = new QPushButton(controlBar);
    m_playPauseBtn->setFocusPolicy(Qt::NoFocus);
    m_playPauseBtn->setFixedWidth(34);
    m_playPauseBtn->setIcon(OmniIcons::playIcon(16));
    m_playPauseBtn->setIconSize(QSize(16, 16));
    m_playPauseBtn->setToolTip(QStringLiteral("Play / Pause (Space)"));
    connect(m_playPauseBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::togglePlayPause);
    bottomRow->addWidget(m_playPauseBtn);

    m_stopBtn = new QPushButton(controlBar);
    m_stopBtn->setFocusPolicy(Qt::NoFocus);
    m_stopBtn->setFixedWidth(28);
    m_stopBtn->setIcon(OmniIcons::stopIcon(14));
    m_stopBtn->setIconSize(QSize(14, 14));
    m_stopBtn->setToolTip(QStringLiteral("Stop"));
    connect(m_stopBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::stop);
    bottomRow->addWidget(m_stopBtn);

    m_prevBtn = new QPushButton(controlBar);
    m_prevBtn->setFocusPolicy(Qt::NoFocus);
    m_prevBtn->setFixedWidth(28);
    m_prevBtn->setIcon(OmniIcons::prevIcon(14));
    m_prevBtn->setIconSize(QSize(14, 14));
    m_prevBtn->setToolTip(QStringLiteral("Previous Item / Seek -5s"));
    connect(m_prevBtn, &QPushButton::clicked, [this]() {
        if (m_playlistManager->hasPrevious()) {
            m_playlistManager->previous();
        } else {
            m_engine->seekRelative(-5.0);
        }
    });
    bottomRow->addWidget(m_prevBtn);

    m_nextBtn = new QPushButton(controlBar);
    m_nextBtn->setFocusPolicy(Qt::NoFocus);
    m_nextBtn->setFixedWidth(28);
    m_nextBtn->setIcon(OmniIcons::nextIcon(14));
    m_nextBtn->setIconSize(QSize(14, 14));
    m_nextBtn->setToolTip(QStringLiteral("Next Item / Seek +5s"));
    connect(m_nextBtn, &QPushButton::clicked, [this]() {
        if (m_playlistManager->hasNext()) {
            m_playlistManager->next();
        } else {
            m_engine->seekRelative(5.0);
        }
    });
    bottomRow->addWidget(m_nextBtn);

    m_openBtn = new QPushButton(controlBar);
    m_openBtn->setFocusPolicy(Qt::NoFocus);
    m_openBtn->setFixedWidth(28);
    m_openBtn->setIcon(OmniIcons::openIcon(14));
    m_openBtn->setIconSize(QSize(14, 14));
    m_openBtn->setToolTip(QStringLiteral("Open File... (Ctrl+O)"));
    connect(m_openBtn, &QPushButton::clicked, this, &MainWindow::promptOpenFile);
    bottomRow->addWidget(m_openBtn);

    // Time Label (Clickable to switch Elapsed vs Remaining)
    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), controlBar);
    m_timeLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; color: #CCCCCC; padding-left: 4px;"));
    m_timeLabel->setCursor(Qt::PointingHandCursor);
    m_timeLabel->setToolTip(QStringLiteral("Click to toggle Remaining Time"));
    bottomRow->addWidget(m_timeLabel);

    // Codec & Status Badges (Interactive)
    m_hwSwBadge = new QLabel(QStringLiteral("S/W"), controlBar);
    m_hwSwBadge->setObjectName(QStringLiteral("badge"));
    m_hwSwBadge->setCursor(Qt::PointingHandCursor);
    m_hwSwBadge->setToolTip(QStringLiteral("Hardware Acceleration (Click to toggle H/W vs S/W)"));
    bottomRow->addWidget(m_hwSwBadge);

    m_hdrBadge = new QLabel(QStringLiteral("HDR"), controlBar);
    m_hdrBadge->setObjectName(QStringLiteral("badge"));
    m_hdrBadge->setCursor(Qt::PointingHandCursor);
    m_hdrBadge->setToolTip(QStringLiteral("High Dynamic Range (Click to toggle Playback Info)"));
    m_hdrBadge->hide();
    bottomRow->addWidget(m_hdrBadge);

    m_videoCodecBadge = new QLabel(controlBar);
    m_videoCodecBadge->setObjectName(QStringLiteral("badge"));
    m_videoCodecBadge->setCursor(Qt::PointingHandCursor);
    m_videoCodecBadge->setToolTip(QStringLiteral("Video Codec (Click to toggle Playback Info)"));
    m_videoCodecBadge->hide();
    bottomRow->addWidget(m_videoCodecBadge);

    m_audioCodecBadge = new QLabel(controlBar);
    m_audioCodecBadge->setObjectName(QStringLiteral("badge"));
    m_audioCodecBadge->setCursor(Qt::PointingHandCursor);
    m_audioCodecBadge->setToolTip(QStringLiteral("Audio Codec (Click to Cycle Audio Track)"));
    m_audioCodecBadge->hide();
    bottomRow->addWidget(m_audioCodecBadge);

    m_channelsBadge = new QLabel(controlBar);
    m_channelsBadge->setObjectName(QStringLiteral("badge"));
    m_channelsBadge->setCursor(Qt::PointingHandCursor);
    m_channelsBadge->setToolTip(QStringLiteral("Audio Channels (Click to Toggle Normalizer)"));
    m_channelsBadge->hide();
    bottomRow->addWidget(m_channelsBadge);

    bottomRow->addStretch(1);

    // Volume & Mute
    m_muteBtn = new QPushButton(controlBar);
    m_muteBtn->setFocusPolicy(Qt::NoFocus);
    m_muteBtn->setFixedWidth(28);
    m_muteBtn->setIcon(OmniIcons::volumeIcon(m_engine->volume(), m_engine->isMuted(), 14));
    m_muteBtn->setIconSize(QSize(14, 14));
    m_muteBtn->setToolTip(QStringLiteral("Mute / Unmute (M)"));
    connect(m_muteBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::toggleMute);
    bottomRow->addWidget(m_muteBtn);

    m_volumeSlider = new QSlider(Qt::Horizontal, controlBar);
    m_volumeSlider->setFocusPolicy(Qt::NoFocus);
    m_volumeSlider->setRange(0, 200);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setToolTip(QStringLiteral("Volume: 100% (0% - 200%)"));
    connect(m_volumeSlider, &QSlider::valueChanged, m_engine, &PlaybackEngine::setVolume);
    bottomRow->addWidget(m_volumeSlider);

    // Playlist Drawer Toggle Button
    m_playlistBtn = new QPushButton(controlBar);
    m_playlistBtn->setFocusPolicy(Qt::NoFocus);
    m_playlistBtn->setCheckable(true);
    m_playlistBtn->setFixedWidth(28);
    m_playlistBtn->setIcon(OmniIcons::playlistIcon(14));
    m_playlistBtn->setIconSize(QSize(14, 14));
    m_playlistBtn->setToolTip(QStringLiteral("Playlist Drawer (F8)"));
    connect(m_playlistBtn, &QPushButton::clicked, this, &MainWindow::togglePlaylistDrawer);
    bottomRow->addWidget(m_playlistBtn);

    // Control Box Toggle Button
    m_controlBoxBtn = new QPushButton(controlBar);
    m_controlBoxBtn->setFocusPolicy(Qt::NoFocus);
    m_controlBoxBtn->setFixedWidth(28);
    m_controlBoxBtn->setIcon(OmniIcons::controlBoxIcon(14));
    m_controlBoxBtn->setIconSize(QSize(14, 14));
    m_controlBoxBtn->setToolTip(QStringLiteral("Control Box (F6)"));
    connect(m_controlBoxBtn, &QPushButton::clicked, this, &MainWindow::toggleControlBox);
    bottomRow->addWidget(m_controlBoxBtn);

    // Fullscreen Toggle
    m_fullscreenBtn = new QPushButton(controlBar);
    m_fullscreenBtn->setFocusPolicy(Qt::NoFocus);
    m_fullscreenBtn->setFixedWidth(28);
    m_fullscreenBtn->setIcon(OmniIcons::fullscreenIcon(isFullScreen(), 14));
    m_fullscreenBtn->setIconSize(QSize(14, 14));
    m_fullscreenBtn->setToolTip(QStringLiteral("Toggle Fullscreen (Enter / Middle Click)"));
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    bottomRow->addWidget(m_fullscreenBtn);

    // Install event filter on all control bar widgets for top-aligned, clean visible tooltips
    for (auto *w : {static_cast<QWidget*>(m_playPauseBtn), static_cast<QWidget*>(m_stopBtn),
                    static_cast<QWidget*>(m_prevBtn), static_cast<QWidget*>(m_nextBtn),
                    static_cast<QWidget*>(m_openBtn), static_cast<QWidget*>(m_timeLabel),
                    static_cast<QWidget*>(m_hwSwBadge), static_cast<QWidget*>(m_hdrBadge),
                    static_cast<QWidget*>(m_videoCodecBadge), static_cast<QWidget*>(m_audioCodecBadge),
                    static_cast<QWidget*>(m_channelsBadge),
                    static_cast<QWidget*>(m_controlBoxBtn), static_cast<QWidget*>(m_playlistBtn),
                    static_cast<QWidget*>(m_muteBtn), static_cast<QWidget*>(m_volumeSlider),
                    static_cast<QWidget*>(m_fullscreenBtn)}) {
        w->installEventFilter(this);
    }

    controlBarLayout->addLayout(bottomRow);
    rootLayout->addWidget(controlBar);

    setCentralWidget(m_centralContainer);
    m_centralContainer->installEventFilter(this);
}

void MainWindow::setupContextMenu() {
    m_contextMenu = new QMenu(this);
    m_contextMenu->setStyleSheet(QStringLiteral(
        "QMenu { background-color: #222222; border: 1px solid #444444; color: #E0E0E0; }"
        "QMenu::item { padding: 4px 20px; }"
        "QMenu::item:selected { background-color: #383838; }"
        "QMenu::separator { height: 1px; background: #333333; margin: 4px 0; }"
    ));

    m_contextMenu->addAction(QStringLiteral("Open File...\tCtrl+O"), this, &MainWindow::promptOpenFile);
    m_contextMenu->addAction(QStringLiteral("Open URL...\tCtrl+U"), this, &MainWindow::promptOpenUrl);
    m_contextMenu->addAction(QStringLiteral("Toggle Playlist\tF8"), this, &MainWindow::togglePlaylistDrawer);
    m_contextMenu->addSeparator();

    m_contextMenu->addAction(QStringLiteral("Play / Pause\tSpace"), m_engine, &PlaybackEngine::togglePlayPause);
    m_contextMenu->addAction(QStringLiteral("Stop"), m_engine, &PlaybackEngine::stop);
    m_contextMenu->addAction(QStringLiteral("Previous Item\tPageUp"), m_playlistManager, &PlaylistManager::previous);
    m_contextMenu->addAction(QStringLiteral("Next Item\tPageDown"), m_playlistManager, &PlaylistManager::next);
    m_contextMenu->addSeparator();

    m_contextMenu->addAction(QStringLiteral("Fullscreen\tEnter"), this, &MainWindow::toggleFullscreen);
    m_stayOnTopAct = m_contextMenu->addAction(QStringLiteral("Always on Top"));
    m_stayOnTopAct->setCheckable(true);
    m_stayOnTopAct->setChecked(m_isStayOnTop);
    connect(m_stayOnTopAct, &QAction::toggled, this, &MainWindow::toggleStayOnTop);

    m_contextMenu->addAction(QStringLiteral("Control Box...\tF6"), this, &MainWindow::toggleControlBox);
    m_contextMenu->addAction(QStringLiteral("Preferences...\tF5"), this, &MainWindow::openPreferencesDialog);
    m_contextMenu->addAction(QStringLiteral("Playback Info\tTab"), this, &MainWindow::toggleInfoOverlay);
    m_contextMenu->addSeparator();

    auto *aspectMenu = m_contextMenu->addMenu(QStringLiteral("Aspect Ratio"));
    aspectMenu->addAction(QStringLiteral("Original"), [this]() { m_engine->setAspectRatio(QStringLiteral("-1")); });
    aspectMenu->addAction(QStringLiteral("16:9"), [this]() { m_engine->setAspectRatio(QStringLiteral("16:9")); });
    aspectMenu->addAction(QStringLiteral("4:3"), [this]() { m_engine->setAspectRatio(QStringLiteral("4:3")); });
    aspectMenu->addAction(QStringLiteral("16:10"), [this]() { m_engine->setAspectRatio(QStringLiteral("16:10")); });
    aspectMenu->addAction(QStringLiteral("2.35:1"), [this]() { m_engine->setAspectRatio(QStringLiteral("2.35:1")); });

    auto *speedMenu = m_contextMenu->addMenu(QStringLiteral("Playback Speed"));
    speedMenu->addAction(QStringLiteral("Normal (1.0x)\tZ"), [this]() { m_engine->setSpeed(1.0); });
    speedMenu->addAction(QStringLiteral("Faster (+0.1x)\tC"), [this]() { m_engine->setSpeed(m_engine->speed() + 0.1); });
    speedMenu->addAction(QStringLiteral("Slower (-0.1x)\tX"), [this]() { m_engine->setSpeed(m_engine->speed() - 0.1); });

    // Dynamic Chapters & Bookmarks Menus
    m_chaptersMenu = m_contextMenu->addMenu(QStringLiteral("Chapters"));
    m_bookmarksMenu = m_contextMenu->addMenu(QStringLiteral("Bookmarks"));
    rebuildChaptersMenu();
    rebuildBookmarksMenu();

    // A-B Repeat Menu
    auto *repeatMenu = m_contextMenu->addMenu(QStringLiteral("A-B Repeat"));
    repeatMenu->addAction(QStringLiteral("Set Point A\t["), [this]() {
        m_engine->setLoopA(m_currentPosition);
        m_timeBar->setRepeatA(m_currentPosition);
    });
    repeatMenu->addAction(QStringLiteral("Set Point B\t]"), [this]() {
        m_engine->setLoopB(m_currentPosition);
        m_timeBar->setRepeatB(m_currentPosition);
    });
    repeatMenu->addAction(QStringLiteral("Clear Repeat\t\\"), [this]() {
        m_engine->clearLoop();
        m_timeBar->clearRepeat();
    });

    // Video Transform Menu
    auto *transMenu = m_contextMenu->addMenu(QStringLiteral("Transform"));
    transMenu->addAction(QStringLiteral("Rotate 90° Clockwise"), m_engine, &PlaybackEngine::rotateClockwise);
    auto *hflipAct = transMenu->addAction(QStringLiteral("Horizontal Flip"));
    hflipAct->setCheckable(true);
    connect(hflipAct, &QAction::toggled, m_engine, &PlaybackEngine::setHorizontalFlip);
    auto *vflipAct = transMenu->addAction(QStringLiteral("Vertical Flip"));
    vflipAct->setCheckable(true);
    connect(vflipAct, &QAction::toggled, m_engine, &PlaybackEngine::setVerticalFlip);
    auto *deintAct = transMenu->addAction(QStringLiteral("Deinterlace (BwDif)"));
    deintAct->setCheckable(true);
    connect(deintAct, &QAction::toggled, m_engine, &PlaybackEngine::setDeinterlacing);

    // Dynamic Track Menus
    m_audioTracksMenu = m_contextMenu->addMenu(QStringLiteral("Audio"));
    m_subtitleTracksMenu = m_contextMenu->addMenu(QStringLiteral("Subtitles"));
    rebuildTrackMenus();

    // Shaders & Capture Menus
    m_shadersMenu = m_contextMenu->addMenu(QStringLiteral("Pixel Shaders"));
    rebuildShaderMenu();

    m_captureMenu = m_contextMenu->addMenu(QStringLiteral("Capture"));
    rebuildCaptureMenu();

    m_contextMenu->addSeparator();
    m_contextMenu->addAction(QStringLiteral("Exit\tCtrl+Q"), this, &QWidget::close);
}

void MainWindow::rebuildTrackMenus() {
    if (!m_audioTracksMenu || !m_subtitleTracksMenu) return;

    // 1. Audio Track Menu
    m_audioTracksMenu->clear();
    auto *audioStreamMenu = m_audioTracksMenu->addMenu(QStringLiteral("Select Audio Stream"));
    const auto audioTracks = m_engine->audioTracks();
    if (audioTracks.isEmpty()) {
        auto *noneAct = audioStreamMenu->addAction(QStringLiteral("No audio tracks"));
        noneAct->setEnabled(false);
    } else {
        for (const auto &track : audioTracks) {
            QString label = QStringLiteral("[%1] %2 (%3, %4)").arg(
                QString::number(track.id),
                track.title.isEmpty() ? QStringLiteral("Track %1").arg(track.id) : track.title,
                track.language.isEmpty() ? QStringLiteral("UND") : track.language,
                track.codec);
            auto *act = audioStreamMenu->addAction(label, [this, id = track.id]() {
                m_engine->setAudioTrack(id);
            });
            act->setCheckable(true);
            act->setChecked(track.isSelected);
        }
    }

    auto *audioDevMenu = m_audioTracksMenu->addMenu(QStringLiteral("Audio Device"));
    const auto devices = m_engine->audioDevices();
    QString curDev = m_engine->currentAudioDevice();
    for (const auto &devStr : devices) {
        QStringList parts = devStr.split(QLatin1Char('|'));
        QString name = parts.value(0);
        QString desc = parts.size() > 1 ? parts.value(1) : name;
        auto *devAct = audioDevMenu->addAction(desc, [this, name]() {
            m_engine->setAudioDevice(name);
        });
        devAct->setCheckable(true);
        devAct->setChecked(name == curDev || (curDev.isEmpty() && name == QStringLiteral("auto")));
    }

    auto *visAct = m_audioTracksMenu->addAction(QStringLiteral("Sound Spectrum Visualizer"));
    visAct->setCheckable(true);
    visAct->setChecked(m_videoWidget->isVisualizerEnabled());
    connect(visAct, &QAction::toggled, m_videoWidget, &VideoWidget::setVisualizerEnabled);

    m_audioTracksMenu->addAction(QStringLiteral("Load External Audio File..."), [this]() {
        QString filter = QStringLiteral("Audio Files (*.mp3 *.aac *.flac *.wav *.ogg *.opus *.m4a);;All Files (*.*)");
        QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load External Audio Track"), QString(), filter);
        if (!path.isEmpty() && m_engine->rawMpvHandle()) {
            const QByteArray utf8Path = path.toUtf8();
            const char *cmd[] = {"audio-add", utf8Path.constData(), "select", nullptr};
            mpv_command(m_engine->rawMpvHandle(), cmd);
        }
    });
    m_audioTracksMenu->addSeparator();

    m_audioTracksMenu->addAction(QStringLiteral("Cycle Audio Track\tA"), m_engine, &PlaybackEngine::cycleAudioTrack);
    m_audioTracksMenu->addAction(QStringLiteral("Audio Delay -50ms\tShift+<"), [this]() {
        double d = m_engine->audioDelay() - 0.05;
        m_engine->setAudioDelay(d);
        int ms = static_cast<int>(std::round(d * 1000.0));
        m_engine->showOsdText(QStringLiteral("Audio Sync : %1%2 ms").arg(ms > 0 ? "+" : "").arg(ms));
    });
    m_audioTracksMenu->addAction(QStringLiteral("Audio Delay +50ms\tShift+>"), [this]() {
        double d = m_engine->audioDelay() + 0.05;
        m_engine->setAudioDelay(d);
        int ms = static_cast<int>(std::round(d * 1000.0));
        m_engine->showOsdText(QStringLiteral("Audio Sync : %1%2 ms").arg(ms > 0 ? "+" : "").arg(ms));
    });
    m_audioTracksMenu->addAction(QStringLiteral("Default Audio Delay\tShift+/"), [this]() {
        m_engine->setAudioDelay(0.0);
        m_engine->showOsdText(QStringLiteral("Audio Sync : Default (0 ms)"));
    });
    m_audioTracksMenu->addSeparator();

    auto *normAct = m_audioTracksMenu->addAction(QStringLiteral("Audio Normalizer (Dynamic Volume)\tShift+N"));
    normAct->setCheckable(true);
    normAct->setChecked(m_engine->isNormalizerEnabled());
    connect(normAct, &QAction::toggled, [this](bool checked) {
        m_engine->setNormalizerEnabled(checked);
        m_engine->showOsdText(checked ? QStringLiteral("Audio Normalizer: Enabled") : QStringLiteral("Audio Normalizer: Disabled"));
    });

    // 2. Subtitle Track Menu
    m_subtitleTracksMenu->clear();
    auto *subStreamMenu = m_subtitleTracksMenu->addMenu(QStringLiteral("Select Subtitle Stream"));
    auto *disableSubAct = subStreamMenu->addAction(QStringLiteral("Disable Subtitles"), [this]() {
        m_engine->setSubtitleTrack(-1);
    });
    disableSubAct->setCheckable(true);

    const auto subTracks = m_engine->subtitleTracks();
    bool anySelected = false;
    for (const auto &track : subTracks) {
        QString label = QStringLiteral("[%1] %2 (%3)").arg(
            QString::number(track.id),
            track.title.isEmpty() ? QStringLiteral("Subtitle %1").arg(track.id) : track.title,
            track.language.isEmpty() ? QStringLiteral("UND") : track.language);
        auto *act = subStreamMenu->addAction(label, [this, id = track.id]() {
            m_engine->setSubtitleTrack(id);
        });
        act->setCheckable(true);
        act->setChecked(track.isSelected);
        if (track.isSelected) anySelected = true;
    }
    disableSubAct->setChecked(!anySelected);
    m_subtitleTracksMenu->addSeparator();

    m_subtitleTracksMenu->addAction(QStringLiteral("Load Subtitle...\tAlt+O"), this, &MainWindow::promptLoadLocalSubtitle);
    m_subtitleTracksMenu->addAction(QStringLiteral("Add Subtitle..."), this, &MainWindow::promptLoadLocalSubtitle);
    m_subtitleTracksMenu->addAction(QStringLiteral("Search Subtitles Online...\tShift+S"), this, &MainWindow::openSubtitleSearchDialog);

    if (RealtimeSubtitleController::isAvailable()) {
        auto *realtimeMenu = m_subtitleTracksMenu->addMenu(QStringLiteral("Realtime subtitle"));
        auto *realtimeGroup = new QActionGroup(realtimeMenu);
        const std::pair<RealtimeSubtitleMode, QString> modes[] = {
            {RealtimeSubtitleMode::Off, QStringLiteral("Off")},
            {RealtimeSubtitleMode::Original, QStringLiteral("Original language")},
            {RealtimeSubtitleMode::TranslateToEnglish, QStringLiteral("Translate to English")},
        };
        for (const auto &[mode, label] : modes) {
            auto *act = realtimeMenu->addAction(label);
            act->setCheckable(true);
            act->setChecked(m_realtimeSubtitle->mode() == mode);
            realtimeGroup->addAction(act);
            connect(act, &QAction::triggered, this, [this, mode]() { m_realtimeSubtitle->setMode(mode); });
        }
        realtimeMenu->addSeparator();
        realtimeMenu->addAction(QStringLiteral("Model && Language..."), this, &MainWindow::openPreferencesDialog);
    } else {
        m_subtitleTracksMenu->addAction(QStringLiteral("Realtime subtitle (not in this build)"))->setEnabled(false);
    }
    m_subtitleTracksMenu->addSeparator();
    m_subtitleTracksMenu->addAction(QStringLiteral("Cycle Subtitle Track\tL"), m_engine, &PlaybackEngine::cycleSubtitleTrack);
    m_subtitleTracksMenu->addAction(QStringLiteral("Subtitle Delay -0.1s\t< or ,"), [this]() {
        double d = m_engine->subtitleDelay() - 0.1;
        m_engine->setSubtitleDelay(d);
        m_engine->showOsdText(QStringLiteral("Subtitle Sync : %1%2 Sec.").arg(d > 0 ? "+" : "").arg(QString::number(d, 'f', 2)));
    });
    m_subtitleTracksMenu->addAction(QStringLiteral("Subtitle Delay +0.1s\t> or ."), [this]() {
        double d = m_engine->subtitleDelay() + 0.1;
        m_engine->setSubtitleDelay(d);
        m_engine->showOsdText(QStringLiteral("Subtitle Sync : %1%2 Sec.").arg(d > 0 ? "+" : "").arg(QString::number(d, 'f', 2)));
    });
    m_subtitleTracksMenu->addAction(QStringLiteral("Default Subtitle Delay\t/"), [this]() {
        m_engine->setSubtitleDelay(0.0);
        m_engine->showOsdText(QStringLiteral("Subtitle Sync : Default (0.00 Sec.)"));
    });
}

void MainWindow::onTracksChanged() {
    // Realtime subtitle reloads its track every few seconds; rebuilding would close an open menu.
    // showContextMenu() rebuilds before every popup anyway.
    if (m_contextMenu && m_contextMenu->isVisible()) return;
    rebuildTrackMenus();
}

void MainWindow::onChaptersChanged() {
    rebuildChaptersMenu();
}

void MainWindow::rebuildChaptersMenu() {
    if (!m_chaptersMenu) return;
    m_chaptersMenu->clear();
    const auto chapters = m_engine->chapters();
    if (chapters.isEmpty()) {
        auto *none = m_chaptersMenu->addAction(QStringLiteral("No chapters in media"));
        none->setEnabled(false);
        return;
    }
    m_chaptersMenu->addAction(QStringLiteral("Previous Chapter"), m_engine, &PlaybackEngine::previousChapter);
    m_chaptersMenu->addAction(QStringLiteral("Next Chapter"), m_engine, &PlaybackEngine::nextChapter);
    m_chaptersMenu->addSeparator();
    int current = m_engine->currentChapter();
    for (int i = 0; i < chapters.size(); ++i) {
        const auto &ch = chapters[i];
        QString label = QStringLiteral("%1. %2 (%3)").arg(QString::number(i + 1), ch.title, formatTime(ch.time));
        auto *act = m_chaptersMenu->addAction(label, [this, i]() {
            m_engine->setChapter(i);
        });
        act->setCheckable(true);
        act->setChecked(i == current);
    }
}

void MainWindow::rebuildBookmarksMenu() {
    if (!m_bookmarksMenu) return;
    m_bookmarksMenu->clear();
    m_bookmarksMenu->addAction(QStringLiteral("Add Bookmark at Current Position\tP"), this, &MainWindow::addBookmarkAtCurrentPosition);
    m_bookmarksMenu->addAction(QStringLiteral("Clear All Bookmarks for Current File"), [this]() {
        SettingsManager::instance().clearBookmarks(m_currentFilePath);
        rebuildBookmarksMenu();
    });
    m_bookmarksMenu->addSeparator();

    auto bms = SettingsManager::instance().bookmarks(m_currentFilePath);
    if (bms.isEmpty()) {
        auto *none = m_bookmarksMenu->addAction(QStringLiteral("No bookmarks set"));
        none->setEnabled(false);
    } else {
        for (const auto &bm : bms) {
            QString label = QStringLiteral("%1 (%2)").arg(bm.title, formatTime(bm.time));
            m_bookmarksMenu->addAction(label, [this, t = bm.time]() {
                m_engine->seek(t, true);
            });
        }
    }
}

void MainWindow::rebuildShaderMenu() {
    if (!m_shadersMenu) return;
    m_shadersMenu->clear();

    auto &sm = ShaderManager::instance();
    const QStringList shaders = sm.availableShaders();

    auto *noneAct = m_shadersMenu->addAction(QStringLiteral("Do Not Use (Reset)"), [this]() {
        ShaderManager::instance().clearActiveShaders(m_engine->rawMpvHandle());
        rebuildShaderMenu();
    });
    noneAct->setCheckable(true);
    noneAct->setChecked(sm.activeShaders().isEmpty());
    m_shadersMenu->addSeparator();

    for (const QString &shader : shaders) {
        auto *act = m_shadersMenu->addAction(shader, [this, shader]() {
            ShaderManager::instance().toggleShader(shader, m_engine->rawMpvHandle());
            rebuildShaderMenu();
        });
        act->setCheckable(true);
        act->setChecked(sm.isShaderActive(shader));
    }
}

void MainWindow::rebuildCaptureMenu() {
    if (!m_captureMenu) return;
    m_captureMenu->clear();

    m_captureMenu->addAction(QStringLiteral("Capture Current Frame\tCtrl+E"), this, &MainWindow::captureStillFrame);
    m_captureMenu->addAction(QStringLiteral("Copy Frame to Clipboard\tCtrl+C"), this, &MainWindow::copyFrameToClipboard);
    m_captureMenu->addAction(QStringLiteral("Consecutive Frame Capture...\tCtrl+Alt+E"), this, &MainWindow::openConsecutiveCaptureDialog);
    m_captureMenu->addSeparator();
    m_captureMenu->addAction(QStringLiteral("Extract Audio Stream...\tAlt+A"), this, &MainWindow::openAudioExtractDialog);
    m_captureMenu->addAction(QStringLiteral("Open Capture Directory"), []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(FrameCapture::defaultCaptureDirectory()));
    });
}

void MainWindow::addBookmarkAtCurrentPosition() {
    if (m_currentFilePath.isEmpty()) return;
    SettingsManager::instance().addBookmark(m_currentFilePath, m_currentPosition);
    rebuildBookmarksMenu();
}

void MainWindow::toggleControlBox() {
    if (!m_controlBox) return;
    m_controlBox->syncFromEngine();
    m_controlBox->setVisible(!m_controlBox->isVisible());
    if (m_controlBox->isVisible()) {
        m_controlBox->raise();
        m_controlBox->activateWindow();
    }
}

void MainWindow::toggleInfoOverlay() {
    if (m_infoOverlay) {
        m_infoOverlay->toggleVisible();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_infoOverlay && m_videoWidget) {
        m_infoOverlay->move(16, 16);
    }
}

void MainWindow::onEofReached() {
    // Automatically play next item in playlist when media finishes
    if (m_playlistManager->hasNext()) {
        m_playlistManager->next();
    }
}

void MainWindow::togglePlaylistDrawer() {
    bool isVisible = m_playlistDrawer->isVisible();
    m_playlistDrawer->setVisible(!isVisible);
    m_playlistBtn->setChecked(!isVisible);
}

void MainWindow::openPaths(const QStringList &paths, double startSeconds) {
    const int firstNew = m_playlistManager->count();
    for (const QString &path : paths) {
        m_playlistManager->addPath(path);
    }
    if (m_playlistManager->count() > firstNew) {
        // setCurrentIndex emits currentItemChanged, which opens the file
        m_pendingStart = startSeconds;
        m_playlistManager->setCurrentIndex(firstNew);
    }
}

void MainWindow::openMediaFile(const QString &filePath, double startSeconds) {
    if (filePath.isEmpty()) return;

    auto &settings = SettingsManager::instance();
    saveResumePosition();

    m_currentFilePath = filePath;
    m_currentPosition = 0.0;
    m_currentDuration = 0.0;
    settings.addRecentFile(filePath);
    if (m_videoWidget) {
        m_videoWidget->setIdle(false);
    }
    if (m_titleBar) {
        m_titleBar->setTitle(QFileInfo(filePath).fileName() + QStringLiteral(" - Omnipot"));
    }

    if (startSeconds < 0.0) {
        startSeconds = settings.rememberPlaybackPosition() ? settings.resumePosition(filePath) : 0.0;
    }
    m_realtimeSubtitle->mediaOpened(filePath);
    m_engine->open(filePath, startSeconds);
}

void MainWindow::promptOpenFile() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Media File"),
        QString(),
        QStringLiteral("Media Files (*.mp4 *.mkv *.avi *.webm *.flv *.ts *.mp3 *.flac *.aac *.ogg *.opus *.wav);;All Files (*)")
    );
    if (!file.isEmpty()) {
        openPaths({file});
    }
}

void MainWindow::promptOpenUrl() {
    OpenUrlDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString url = dlg.url();
        if (!url.isEmpty()) {
            QString ua = dlg.userAgent();
            if (!ua.isEmpty() && m_engine->rawMpvHandle()) {
                mpv_set_property_string(m_engine->rawMpvHandle(), "user-agent", ua.toUtf8().constData());
            }
            QString ref = dlg.referrer();
            if (!ref.isEmpty() && m_engine->rawMpvHandle()) {
                mpv_set_property_string(m_engine->rawMpvHandle(), "referrer", ref.toUtf8().constData());
            }

            openPaths({url});
        }
    }
}

void MainWindow::captureStillFrame() {
    QString savedPath;
    if (m_frameCapture->captureFrame(m_engine->rawMpvHandle(), QString(), QStringLiteral("png"), &savedPath)) {
        setWindowTitle(QStringLiteral("Captured: %1 - Omnipot").arg(QFileInfo(savedPath).fileName()));
    }
}

void MainWindow::copyFrameToClipboard() {
    if (m_frameCapture->copyFrameToClipboard(m_engine->rawMpvHandle())) {
        setWindowTitle(QStringLiteral("Frame copied to clipboard! - Omnipot"));
    }
}

void MainWindow::openConsecutiveCaptureDialog() {
    ConsecutiveCaptureDialog dlg(m_frameCapture, m_engine->rawMpvHandle(), this);
    dlg.exec();
}

void MainWindow::openAudioExtractDialog() {
    AudioExtractDialog dlg(m_currentFilePath, this);
    dlg.exec();
}

void MainWindow::openSubtitleSearchDialog() {
    QString title = m_engine->metadata().title;
    if (title.isEmpty()) title = QFileInfo(m_currentFilePath).completeBaseName();
    SubtitleSearchDialog dlg(m_engine->rawMpvHandle(), title, this);
    dlg.exec();
}

void MainWindow::promptLoadLocalSubtitle() {
    QString filter = QStringLiteral("Subtitle Files (*.srt *.ass *.ssa *.vtt *.sub *.smi *.idx *.sup);;All Files (*.*)");
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load Subtitle File (Alt+O)"), QString(), filter);
    if (!path.isEmpty() && m_engine->rawMpvHandle()) {
        const QByteArray utf8Path = path.toUtf8();
        const char *cmd[] = {"sub-add", utf8Path.constData(), "select", nullptr};
        mpv_command(m_engine->rawMpvHandle(), cmd);
        rebuildTrackMenus();
    }
}

void MainWindow::wheelEvent(QWheelEvent *event) {
    int delta = event->angleDelta().y();
    if (delta != 0) {
        int step = (delta > 0) ? 5 : -5;
        int newVol = std::clamp(m_engine->volume() + step, 0, 200);
        m_engine->setVolume(newVol);
        m_engine->showOsdText(QStringLiteral("Volume: %1%").arg(newVol));
        event->accept();
        return;
    }
    QMainWindow::wheelEvent(event);
}

void MainWindow::openPreferencesDialog() {
    PreferencesDialog dlg(m_engine, this);
    dlg.exec();
    // The dialog applies engine settings itself but has no access to the playlist
    applyPlaylistPreferences();
    m_realtimeSubtitle->reloadSettings();
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
        if (m_titleBar) m_titleBar->show();
    } else {
        if (m_titleBar) m_titleBar->hide();
        showFullScreen();
    }
    if (m_fullscreenBtn) {
        m_fullscreenBtn->setIcon(OmniIcons::fullscreenIcon(isFullScreen(), 14));
        m_fullscreenBtn->setToolTip(isFullScreen() ? QStringLiteral("Exit Fullscreen (Enter / Esc / Middle Click)") : QStringLiteral("Fullscreen (Enter / Middle Click)"));
    }
    updateWindowBorder();
}

void MainWindow::showContextMenu(const QPoint &globalPos) {
    if (m_contextMenu) {
        rebuildTrackMenus();
        rebuildChaptersMenu();
        rebuildBookmarksMenu();
        m_contextMenu->popup(globalPos);
    }
}

void MainWindow::toggleTimeDisplayMode() {
    m_showRemainingTime = !m_showRemainingTime;
    onPositionChanged(m_currentPosition);
}

void MainWindow::toggleHardwareAcceleration() {
    bool isHw = (m_hwSwBadge->text() == QStringLiteral("H/W"));
    if (isHw) {
        m_engine->setHwdec(QStringLiteral("no"));
        m_engine->showOsdText(QStringLiteral("Hardware Acceleration: Disabled (Software)"), 2000);
    } else {
        m_engine->setHwdec(QStringLiteral("vaapi,auto-safe"));
        m_engine->showOsdText(QStringLiteral("Hardware Acceleration: Enabled (H/W)"), 2000);
    }
}

void MainWindow::onStateChanged(PlaybackState state) {
    bool isIdle = (state == PlaybackState::Idle || state == PlaybackState::Stopped || state == PlaybackState::Error);
    if (m_videoWidget) {
        m_videoWidget->setIdle(isIdle);
    }
    if (isIdle && m_titleBar) {
        m_titleBar->setTitle(QStringLiteral("Omnipot"));
    }
    switch (state) {
    case PlaybackState::Playing:
        m_playPauseBtn->setIcon(OmniIcons::pauseIcon(16));
        m_playPauseBtn->setToolTip(QStringLiteral("Pause (Space)"));
        break;
    case PlaybackState::Paused:
    case PlaybackState::Stopped:
    case PlaybackState::Idle:
    case PlaybackState::Error:
        m_playPauseBtn->setIcon(OmniIcons::playIcon(16));
        m_playPauseBtn->setToolTip(QStringLiteral("Play (Space)"));
        break;
    default:
        break;
    }
}

void MainWindow::onPositionChanged(double seconds) {
    m_currentPosition = seconds;
    m_timeBar->setPosition(seconds);

    QString text;
    if (m_showRemainingTime && m_currentDuration > 0.0) {
        double remaining = std::max(0.0, m_currentDuration - seconds);
        text = QStringLiteral("-%1 / %2").arg(formatTime(remaining), formatTime(m_currentDuration));
    } else {
        text = QStringLiteral("%1 / %2").arg(formatTime(seconds), formatTime(m_currentDuration));
    }
    m_timeLabel->setText(text);
}

void MainWindow::onDurationChanged(double seconds) {
    m_currentDuration = seconds;
    m_timeBar->setDuration(seconds);
    onPositionChanged(m_currentPosition);
}

void MainWindow::onVolumeChanged(int volume) {
    if (m_volumeSlider && m_volumeSlider->value() != volume) {
        m_volumeSlider->blockSignals(true);
        m_volumeSlider->setValue(volume);
        m_volumeSlider->blockSignals(false);
    }
    m_volumeSlider->setToolTip(QStringLiteral("Volume: %1% (0% - 200%)").arg(volume));
    if (m_muteBtn) {
        m_muteBtn->setIcon(OmniIcons::volumeIcon(volume, m_engine->isMuted(), 14));
    }
}

void MainWindow::onMuteChanged(bool muted) {
    if (m_muteBtn) {
        m_muteBtn->setIcon(OmniIcons::volumeIcon(m_engine->volume(), muted, 14));
        m_muteBtn->setToolTip(muted ? QStringLiteral("Unmute (M)") : QStringLiteral("Mute (M)"));
    }
}

void MainWindow::onSpeedChanged(double /*speed*/) {
    // Reflected in title/OSD
}

void MainWindow::onMetadataChanged(const MediaMetadata &metadata) {
    if (!metadata.title.isEmpty()) {
        QString title = QStringLiteral("%1 - Omnipot").arg(metadata.title);
        setWindowTitle(title);
        if (m_titleBar) {
            m_titleBar->setTitle(title);
        }
    }
    updateBadges(metadata);

    // Auto-enable spectrum visualizer for audio-only media
    if (m_videoWidget) {
        bool isAudioOnly = (metadata.videoWidth == 0 && !m_currentFilePath.isEmpty());
        m_videoWidget->setVisualizerEnabled(isAudioOnly);
    }
}

void MainWindow::updateBadges(const MediaMetadata &meta) {
    m_hwSwBadge->setText(meta.isHardwareDecoded ? QStringLiteral("H/W") : QStringLiteral("S/W"));
    m_hwSwBadge->setStyleSheet(meta.isHardwareDecoded
        ? QStringLiteral("QLabel#badge { background-color: #1A3820; border: 1px solid #287030; color: #44DD66; } "
                         "QLabel#badge:hover { background-color: #224528; border: 1px solid #3AA040; color: #66FF88; }")
        : QStringLiteral("QLabel#badge { background-color: #202020; border: 1px solid #383838; color: #AAAAAA; } "
                         "QLabel#badge:hover { background-color: #2e2e2e; border: 1px solid #555555; color: #FFFFFF; }"));

    if (meta.isHdr) {
        m_hdrBadge->show();
    } else {
        m_hdrBadge->hide();
    }

    if (!meta.videoCodec.isEmpty()) {
        m_videoCodecBadge->setText(meta.videoCodec);
        m_videoCodecBadge->setToolTip(QStringLiteral("Video Codec: %1 (Click for detailed playback info)").arg(meta.videoCodec));
        m_videoCodecBadge->show();
    } else {
        m_videoCodecBadge->hide();
    }

    if (!meta.audioCodec.isEmpty()) {
        m_audioCodecBadge->setText(meta.audioCodec);
        m_audioCodecBadge->setToolTip(QStringLiteral("Audio Codec: %1 (Click to cycle audio track)").arg(meta.audioCodec));
        m_audioCodecBadge->show();
    } else {
        m_audioCodecBadge->hide();
    }

    if (meta.audioChannels > 0) {
        switch (meta.audioChannels) {
        case 1: m_channelsBadge->setText(QStringLiteral("MONO")); break;
        case 2: m_channelsBadge->setText(QStringLiteral("2.0")); break;
        case 6: m_channelsBadge->setText(QStringLiteral("5.1")); break;
        case 8: m_channelsBadge->setText(QStringLiteral("7.1")); break;
        default: m_channelsBadge->setText(QStringLiteral("%1CH").arg(meta.audioChannels)); break;
        }
        m_channelsBadge->setToolTip(QStringLiteral("Audio Channels: %1 (Click to toggle Normalizer)").arg(m_channelsBadge->text()));
        m_channelsBadge->show();
    } else {
        m_channelsBadge->hide();
    }
}

void MainWindow::onErrorOccurred(const QString &error) {
    QMessageBox::warning(this, QStringLiteral("Playback Error"), error);
}

QString MainWindow::formatTime(double seconds) const {
    qint64 totalSec = static_cast<qint64>(std::max(0.0, seconds));
    int h = static_cast<int>(totalSec / 3600);
    int m = static_cast<int>((totalSec % 3600) / 60);
    int s = static_cast<int>(totalSec % 60);

    if (h > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    } else {
        return QStringLiteral("%1:%2")
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Space:
        m_engine->togglePlayPause();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        toggleFullscreen();
        event->accept();
        return;
    case Qt::Key_Escape:
        if (isFullScreen()) {
            toggleFullscreen();
            event->accept();
            return;
        }
        break;
    case Qt::Key_F8:
        togglePlaylistDrawer();
        event->accept();
        return;
    case Qt::Key_PageUp:
        m_playlistManager->previous();
        event->accept();
        return;
    case Qt::Key_PageDown:
        m_playlistManager->next();
        event->accept();
        return;
    case Qt::Key_Left:
        if (event->modifiers() & Qt::ControlModifier) {
            m_engine->seekRelative(-30.0);
        } else {
            m_engine->seekRelative(-5.0);
        }
        event->accept();
        return;
    case Qt::Key_Right:
        if (event->modifiers() & Qt::ControlModifier) {
            m_engine->seekRelative(30.0);
        } else {
            m_engine->seekRelative(5.0);
        }
        event->accept();
        return;
    case Qt::Key_Up: {
        int newVol = std::clamp(m_engine->volume() + 5, 0, 200);
        m_engine->setVolume(newVol);
        m_engine->showOsdText(QStringLiteral("Volume: %1%").arg(newVol));
        event->accept();
        return;
    }
    case Qt::Key_Down: {
        int newVol = std::clamp(m_engine->volume() - 5, 0, 200);
        m_engine->setVolume(newVol);
        m_engine->showOsdText(QStringLiteral("Volume: %1%").arg(newVol));
        event->accept();
        return;
    }
    case Qt::Key_M:
        m_engine->toggleMute();
        event->accept();
        return;
    case Qt::Key_N:
        if (event->modifiers() & Qt::ShiftModifier) {
            bool newState = !m_engine->isNormalizerEnabled();
            m_engine->setNormalizerEnabled(newState);
            m_engine->showOsdText(newState ? QStringLiteral("Audio Normalizer: Enabled") : QStringLiteral("Audio Normalizer: Disabled"));
            event->accept();
            return;
        }
        break;
    case Qt::Key_A:
        if (event->modifiers() & Qt::AltModifier) {
            openAudioExtractDialog();
            event->accept();
            return;
        } else {
            m_engine->cycleAudioTrack();
            event->accept();
            return;
        }
    case Qt::Key_L:
        m_engine->cycleSubtitleTrack();
        event->accept();
        return;
    case Qt::Key_Z:
        m_engine->setSpeed(1.0);
        m_engine->showOsdText(QStringLiteral("Playback Speed: 1.00x"));
        event->accept();
        return;
    case Qt::Key_X: {
        double spd = std::max(0.2, m_engine->speed() - 0.1);
        m_engine->setSpeed(spd);
        m_engine->showOsdText(QStringLiteral("Playback Speed: %1x").arg(QString::number(spd, 'f', 2)));
        event->accept();
        return;
    }
    case Qt::Key_C:
        if (event->modifiers() & Qt::ControlModifier) {
            copyFrameToClipboard();
            event->accept();
            return;
        } else {
            double spd = std::min(12.0, m_engine->speed() + 0.1);
            m_engine->setSpeed(spd);
            m_engine->showOsdText(QStringLiteral("Playback Speed: %1x").arg(QString::number(spd, 'f', 2)));
            event->accept();
            return;
        }
    case Qt::Key_Less:
    case Qt::Key_Comma:
        if ((event->modifiers() & Qt::ShiftModifier) || event->key() == Qt::Key_Less) {
            double d = m_engine->audioDelay() - 0.05;
            m_engine->setAudioDelay(d);
            int ms = static_cast<int>(std::round(d * 1000.0));
            m_engine->showOsdText(QStringLiteral("Audio Sync : %1%2 ms").arg(ms > 0 ? "+" : "").arg(ms));
        } else {
            double d = m_engine->subtitleDelay() - 0.1;
            m_engine->setSubtitleDelay(d);
            m_engine->showOsdText(QStringLiteral("Subtitle Sync : %1%2 Sec.").arg(d > 0 ? "+" : "").arg(QString::number(d, 'f', 2)));
        }
        event->accept();
        return;
    case Qt::Key_Greater:
    case Qt::Key_Period:
        if ((event->modifiers() & Qt::ShiftModifier) || event->key() == Qt::Key_Greater) {
            double d = m_engine->audioDelay() + 0.05;
            m_engine->setAudioDelay(d);
            int ms = static_cast<int>(std::round(d * 1000.0));
            m_engine->showOsdText(QStringLiteral("Audio Sync : %1%2 ms").arg(ms > 0 ? "+" : "").arg(ms));
        } else {
            double d = m_engine->subtitleDelay() + 0.1;
            m_engine->setSubtitleDelay(d);
            m_engine->showOsdText(QStringLiteral("Subtitle Sync : %1%2 Sec.").arg(d > 0 ? "+" : "").arg(QString::number(d, 'f', 2)));
        }
        event->accept();
        return;
    case Qt::Key_Question:
    case Qt::Key_Slash:
        if ((event->modifiers() & Qt::ShiftModifier) || event->key() == Qt::Key_Question) {
            m_engine->setAudioDelay(0.0);
            m_engine->showOsdText(QStringLiteral("Audio Sync : Default (0 ms)"));
        } else {
            m_engine->setSubtitleDelay(0.0);
            m_engine->showOsdText(QStringLiteral("Subtitle Sync : Default (0.00 Sec.)"));
        }
        event->accept();
        return;
    case Qt::Key_E:
        if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::AltModifier)) {
            openConsecutiveCaptureDialog();
            event->accept();
            return;
        } else if (event->modifiers() & Qt::ControlModifier) {
            captureStillFrame();
            event->accept();
            return;
        }
        break;
    case Qt::Key_S:
        if (event->modifiers() & Qt::ShiftModifier) {
            openSubtitleSearchDialog();
            event->accept();
            return;
        }
        break;
    case Qt::Key_O:
        if (event->modifiers() & Qt::AltModifier) {
            promptLoadLocalSubtitle();
            event->accept();
            return;
        } else if (event->modifiers() & Qt::ControlModifier) {
            promptOpenFile();
            event->accept();
            return;
        }
        break;
    case Qt::Key_U:
        if (event->modifiers() & Qt::ControlModifier) {
            promptOpenUrl();
            event->accept();
            return;
        }
        break;
    case Qt::Key_D:
        m_engine->frameBackStep();
        event->accept();
        return;
    case Qt::Key_F:
        m_engine->frameStep();
        event->accept();
        return;
    case Qt::Key_F5:
        openPreferencesDialog();
        event->accept();
        return;
    case Qt::Key_F6:
        toggleControlBox();
        event->accept();
        return;
    case Qt::Key_F1:
        showContextMenu(m_titleBar && m_titleBar->isVisible()
            ? m_titleBar->menuButton()->mapToGlobal(QPoint(0, m_titleBar->menuButton()->height()))
            : QCursor::pos());
        event->accept();
        return;
    case Qt::Key_Q:
        if (event->modifiers() & Qt::ControlModifier) {
            close();
            event->accept();
            return;
        }
        break;
    case Qt::Key_P:
        addBookmarkAtCurrentPosition();
        event->accept();
        return;
    case Qt::Key_BracketLeft:
        m_engine->setLoopA(m_currentPosition);
        m_timeBar->setRepeatA(m_currentPosition);
        event->accept();
        return;
    case Qt::Key_BracketRight:
        m_engine->setLoopB(m_currentPosition);
        m_timeBar->setRepeatB(m_currentPosition);
        event->accept();
        return;
    case Qt::Key_Backslash:
        m_engine->clearLoop();
        m_timeBar->clearRepeat();
        event->accept();
        return;
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        for (const auto &u : urls) {
            QString localPath = u.toLocalFile();
            if (!localPath.isEmpty()) {
                m_playlistManager->addPath(localPath);
            } else {
                m_playlistManager->addPath(u.toString());
            }
        }
        if (m_playlistManager->count() > 0 && m_playlistManager->currentIndex() == -1) {
            m_playlistManager->setCurrentIndex(0);
        }
        event->acceptProposedAction();
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
        Qt::Edges edges = calculateResizeEdges(event->pos());
        if (edges != Qt::Edges()) {
            if (windowHandle() && windowHandle()->startSystemResize(edges)) {
                event->accept();
                return;
            }
        }
    } else if (event->button() == Qt::MiddleButton) {
        toggleFullscreen();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!isMaximized() && !isFullScreen()) {
        Qt::Edges edges = calculateResizeEdges(event->pos());
        updateCursorShape(edges);
    } else {
        unsetCursor();
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange) {
        bool max = isMaximized();
        bool fs = isFullScreen();
        if (m_titleBar) {
            m_titleBar->setMaximized(max);
            m_titleBar->setVisible(!fs);
        }
        updateWindowBorder();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::toggleStayOnTop(bool enable) {
    m_isStayOnTop = enable;
    setWindowFlag(Qt::WindowStaysOnTopHint, enable);
    show();
    if (m_titleBar) {
        m_titleBar->setStayOnTop(enable);
    }
    if (m_stayOnTopAct) {
        const QSignalBlocker blocker(m_stayOnTopAct);
        m_stayOnTopAct->setChecked(enable);
    }
    m_engine->showOsdText(enable ? QStringLiteral("Always on Top: Enabled") : QStringLiteral("Always on Top: Disabled"));
}

void MainWindow::updateWindowBorder() {
    if (!m_centralContainer) return;
    QString borderStyle = (isMaximized() || isFullScreen()) ? QStringLiteral("border: none;") : QStringLiteral("border: 1px solid #2b2b2b;");
    m_centralContainer->setStyleSheet(QStringLiteral(
        "QWidget#centralContainer { background-color: #121212; color: #E0E0E0; font-family: 'Segoe UI', 'Ubuntu', 'Sans-Serif'; font-size: 11px; %1 }"
        "QPushButton { background-color: #242424; color: #E0E0E0; border: 1px solid #333333; border-radius: 2px; padding: 3px 8px; min-height: 18px; }"
        "QPushButton:hover { background-color: #383838; border-color: #555555; color: #FFFFFF; }"
        "QPushButton:pressed { background-color: #1a1a1a; color: #888888; }"
        "QPushButton:checked { background-color: #4A1A1A; border-color: #AA2222; color: #FFFFFF; }"
        "QWidget#controlBar { background-color: #161616; border-top: 1px solid #262626; }"
        "QWidget#controlBar QPushButton { background-color: #202020; border: 1px solid #2e2e2e; border-radius: 2px; padding: 2px; min-height: 22px; color: #E0E0E0; }"
        "QWidget#controlBar QPushButton:hover { background-color: #353535; border: 1px solid #4a4a4a; color: #FFFFFF; }"
        "QWidget#controlBar QPushButton:pressed { background-color: #141414; border: 1px solid #222222; color: #888888; }"
        "QWidget#controlBar QPushButton:checked { background-color: #382A1A; border: 1px solid #F0A020; color: #FFFFFF; }"
        "QLabel#badge { background-color: #202020; border: 1px solid #383838; border-radius: 2px; padding: 1px 4px; font-weight: bold; font-size: 10px; color: #AAAAAA; }"
        "QLabel#badge:hover { background-color: #2e2e2e; border: 1px solid #555555; color: #FFFFFF; }"
        "QSlider::groove:horizontal { height: 4px; background: #2B2B2B; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #E5A823; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #FFFFFF; border: 1px solid #888888; width: 10px; margin-top: -3px; margin-bottom: -3px; border-radius: 5px; }"
        "QToolTip { background-color: #242424; color: #FFFFFF; border: 1px solid #F0A020; border-radius: 4px; padding: 4px 8px; font-size: 11px; font-weight: 500; }"
    ).arg(borderStyle));
}

Qt::Edges MainWindow::calculateResizeEdges(const QPoint &pos) const {
    if (isMaximized() || isFullScreen()) {
        return Qt::Edges();
    }
    Qt::Edges edges = Qt::Edges();
    const int margin = 6;
    if (pos.x() <= margin) edges |= Qt::LeftEdge;
    if (pos.x() >= width() - margin) edges |= Qt::RightEdge;
    if (pos.y() <= margin) edges |= Qt::TopEdge;
    if (pos.y() >= height() - margin) edges |= Qt::BottomEdge;
    return edges;
}

void MainWindow::updateCursorShape(Qt::Edges edges) {
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
        (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
               (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & Qt::LeftEdge || edges & Qt::RightEdge) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & Qt::TopEdge || edges & Qt::BottomEdge) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseMove) {
        auto *mouseEv = static_cast<QMouseEvent*>(event);
        QPoint winPos = mapFromGlobal(mouseEv->globalPosition().toPoint());
        if (!isMaximized() && !isFullScreen()) {
            Qt::Edges edges = calculateResizeEdges(winPos);
            if (edges != Qt::Edges()) {
                updateCursorShape(edges);
            } else if (cursor().shape() != Qt::ArrowCursor) {
                unsetCursor();
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEv = static_cast<QMouseEvent*>(event);
        if (mouseEv->button() == Qt::LeftButton) {
            if (watched == m_timeLabel) {
                toggleTimeDisplayMode();
                return true;
            }
            if (watched == m_hwSwBadge) {
                toggleHardwareAcceleration();
                return true;
            }
            if (watched == m_videoCodecBadge || watched == m_hdrBadge) {
                toggleInfoOverlay();
                return true;
            }
            if (watched == m_audioCodecBadge) {
                m_engine->cycleAudioTrack();
                return true;
            }
            if (watched == m_channelsBadge) {
                bool newState = !m_engine->isNormalizerEnabled();
                m_engine->setNormalizerEnabled(newState);
                m_engine->showOsdText(newState ? QStringLiteral("Audio Normalizer: Enabled") : QStringLiteral("Audio Normalizer: Disabled"));
                return true;
            }
        }
        if (mouseEv->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
            QPoint winPos = mapFromGlobal(mouseEv->globalPosition().toPoint());
            Qt::Edges edges = calculateResizeEdges(winPos);
            if (edges != Qt::Edges()) {
                if (windowHandle() && windowHandle()->startSystemResize(edges)) {
                    return true;
                }
            }
        } else if (mouseEv->button() == Qt::MiddleButton) {
            toggleFullscreen();
            return true;
        }
    } else if (event->type() == QEvent::ToolTip) {
        auto *widget = qobject_cast<QWidget*>(watched);
        if (widget && !widget->toolTip().isEmpty()) {
            QPoint widgetTopCenter = widget->mapToGlobal(QPoint(widget->width() / 2, 0));
            QFontMetrics fm(QToolTip::font());
            QString plainTip = widget->toolTip();
            int tipWidth = fm.horizontalAdvance(plainTip) + 24;
            // Position above the control bar, nicely inside the video canvas
            QPoint tipPos(widgetTopCenter.x() - tipWidth / 2, widgetTopCenter.y() - 34);
            QString richTip = QStringLiteral("<span style='color: #FFFFFF; font-weight: bold;'>%1</span>").arg(plainTip.toHtmlEscaped());
            QToolTip::showText(tipPos, richTip, widget);
            return true;
        }
    } else if (event->type() == QEvent::Leave) {
        QToolTip::hideText();
    }
    return QMainWindow::eventFilter(watched, event);
}

} // namespace Omnipot
