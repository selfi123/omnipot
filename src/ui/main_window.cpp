#include "ui/main_window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QStyle>
#include <algorithm>

namespace PotPlayer {

MainWindow::MainWindow(PlaybackEngine *engine, QWidget *parent)
    : QMainWindow(parent)
    , m_engine(engine)
{
    setWindowTitle(QStringLiteral("PotPlayer Linux"));
    resize(1024, 600);
    setMinimumSize(480, 320);
    setAcceptDrops(true);

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
    connect(m_engine, &PlaybackEngine::errorOccurred, this, &MainWindow::onErrorOccurred);

    // Initial state
    onVolumeChanged(m_engine->volume());
    onMuteChanged(m_engine->isMuted());
}

void MainWindow::setupUi() {
    auto *centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #121212; color: #E0E0E0; font-family: 'Segoe UI', 'Ubuntu', 'Sans-Serif'; font-size: 11px; }"
        "QPushButton { background-color: #242424; border: 1px solid #333333; border-radius: 2px; padding: 3px 8px; min-height: 18px; }"
        "QPushButton:hover { background-color: #383838; border-color: #555555; }"
        "QPushButton:pressed { background-color: #1a1a1a; }"
        "QLabel#badge { background-color: #202020; border: 1px solid #383838; border-radius: 2px; padding: 1px 4px; font-weight: bold; font-size: 10px; color: #AAAAAA; }"
        "QSlider::groove:horizontal { height: 4px; background: #333333; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #CC2222; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #FFFFFF; border: 1px solid #888888; width: 10px; margin-top: -3px; margin-bottom: -3px; border-radius: 5px; }"
    ));

    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 1. Video Canvas
    m_videoWidget = new VideoWidget(centralWidget);
    connect(m_videoWidget, &VideoWidget::clicked, m_engine, &PlaybackEngine::togglePlayPause);
    connect(m_videoWidget, &VideoWidget::doubleClicked, this, &MainWindow::toggleFullscreen);
    connect(m_videoWidget, &VideoWidget::rightClicked, this, &MainWindow::showContextMenu);
    connect(m_videoWidget, &VideoWidget::wheelScrolled, [this](int delta) {
        int step = (delta > 0) ? 2 : -2;
        m_engine->setVolume(m_engine->volume() + step);
    });
    rootLayout->addWidget(m_videoWidget, 1);

    // 2. Control Bar Container
    auto *controlBar = new QWidget(centralWidget);
    controlBar->setFixedHeight(58);
    controlBar->setStyleSheet(QStringLiteral("background-color: #181818; border-top: 1px solid #282828;"));

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

    m_playPauseBtn = new QPushButton(QStringLiteral("▶"), controlBar);
    m_playPauseBtn->setFixedWidth(34);
    m_playPauseBtn->setToolTip(QStringLiteral("Play / Pause (Space)"));
    connect(m_playPauseBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::togglePlayPause);
    bottomRow->addWidget(m_playPauseBtn);

    m_stopBtn = new QPushButton(QStringLiteral("⏹"), controlBar);
    m_stopBtn->setFixedWidth(28);
    m_stopBtn->setToolTip(QStringLiteral("Stop"));
    connect(m_stopBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::stop);
    bottomRow->addWidget(m_stopBtn);

    m_prevBtn = new QPushButton(QStringLiteral("⏮"), controlBar);
    m_prevBtn->setFixedWidth(28);
    m_prevBtn->setToolTip(QStringLiteral("Seek -5s (Left Arrow)"));
    connect(m_prevBtn, &QPushButton::clicked, [this]() { m_engine->seekRelative(-5.0); });
    bottomRow->addWidget(m_prevBtn);

    m_nextBtn = new QPushButton(QStringLiteral("⏭"), controlBar);
    m_nextBtn->setFixedWidth(28);
    m_nextBtn->setToolTip(QStringLiteral("Seek +5s (Right Arrow)"));
    connect(m_nextBtn, &QPushButton::clicked, [this]() { m_engine->seekRelative(5.0); });
    bottomRow->addWidget(m_nextBtn);

    m_openBtn = new QPushButton(QStringLiteral("📁"), controlBar);
    m_openBtn->setFixedWidth(28);
    m_openBtn->setToolTip(QStringLiteral("Open File... (Ctrl+O)"));
    connect(m_openBtn, &QPushButton::clicked, this, &MainWindow::promptOpenFile);
    bottomRow->addWidget(m_openBtn);

    // Time Label (Clickable to switch Elapsed vs Remaining)
    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), controlBar);
    m_timeLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; color: #CCCCCC; padding-left: 4px;"));
    m_timeLabel->setCursor(Qt::PointingHandCursor);
    m_timeLabel->setToolTip(QStringLiteral("Click to toggle Remaining Time"));
    bottomRow->addWidget(m_timeLabel);

    // Codec & Status Badges
    m_hwSwBadge = new QLabel(QStringLiteral("S/W"), controlBar);
    m_hwSwBadge->setObjectName(QStringLiteral("badge"));
    bottomRow->addWidget(m_hwSwBadge);

    m_hdrBadge = new QLabel(QStringLiteral("HDR"), controlBar);
    m_hdrBadge->setObjectName(QStringLiteral("badge"));
    m_hdrBadge->hide();
    bottomRow->addWidget(m_hdrBadge);

    m_videoCodecBadge = new QLabel(controlBar);
    m_videoCodecBadge->setObjectName(QStringLiteral("badge"));
    m_videoCodecBadge->hide();
    bottomRow->addWidget(m_videoCodecBadge);

    m_audioCodecBadge = new QLabel(controlBar);
    m_audioCodecBadge->setObjectName(QStringLiteral("badge"));
    m_audioCodecBadge->hide();
    bottomRow->addWidget(m_audioCodecBadge);

    m_channelsBadge = new QLabel(controlBar);
    m_channelsBadge->setObjectName(QStringLiteral("badge"));
    m_channelsBadge->hide();
    bottomRow->addWidget(m_channelsBadge);

    bottomRow->addStretch(1);

    // Volume & Mute
    m_muteBtn = new QPushButton(QStringLiteral("🔊"), controlBar);
    m_muteBtn->setFixedWidth(28);
    m_muteBtn->setToolTip(QStringLiteral("Mute / Unmute (M)"));
    connect(m_muteBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::toggleMute);
    bottomRow->addWidget(m_muteBtn);

    m_volumeSlider = new QSlider(Qt::Horizontal, controlBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setToolTip(QStringLiteral("Volume (Up/Down)"));
    connect(m_volumeSlider, &QSlider::valueChanged, m_engine, &PlaybackEngine::setVolume);
    bottomRow->addWidget(m_volumeSlider);

    // Fullscreen Toggle
    m_fullscreenBtn = new QPushButton(QStringLiteral("⛶"), controlBar);
    m_fullscreenBtn->setFixedWidth(28);
    m_fullscreenBtn->setToolTip(QStringLiteral("Toggle Fullscreen (Enter)"));
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    bottomRow->addWidget(m_fullscreenBtn);

    controlBarLayout->addLayout(bottomRow);
    rootLayout->addWidget(controlBar);

    setCentralWidget(centralWidget);
}

void MainWindow::setupContextMenu() {
    m_contextMenu = new QMenu(this);
    m_contextMenu->setStyleSheet(QStringLiteral(
        "QMenu { background-color: #222222; border: 1px solid #444444; color: #E0E0E0; }"
        "QMenu::item { padding: 4px 20px; }"
        "QMenu::item:selected { background-color: #383838; }"
        "QMenu::separator { height: 1px; background: #333333; margin: 4px 0; }"
    ));

    auto *openFileAct = m_contextMenu->addAction(QStringLiteral("Open File...\tCtrl+O"), this, &MainWindow::promptOpenFile);
    auto *openUrlAct = m_contextMenu->addAction(QStringLiteral("Open URL...\tCtrl+U"), this, &MainWindow::promptOpenUrl);
    m_contextMenu->addSeparator();

    auto *playPauseAct = m_contextMenu->addAction(QStringLiteral("Play / Pause\tSpace"), m_engine, &PlaybackEngine::togglePlayPause);
    auto *stopAct = m_contextMenu->addAction(QStringLiteral("Stop"), m_engine, &PlaybackEngine::stop);
    m_contextMenu->addSeparator();

    auto *fullscreenAct = m_contextMenu->addAction(QStringLiteral("Fullscreen\tEnter"), this, &MainWindow::toggleFullscreen);

    auto *aspectMenu = m_contextMenu->addMenu(QStringLiteral("Aspect Ratio"));
    aspectMenu->addAction(QStringLiteral("Original"), [this]() { m_engine->setAspectRatio(QStringLiteral("-1")); });
    aspectMenu->addAction(QStringLiteral("16:9"), [this]() { m_engine->setAspectRatio(QStringLiteral("16:9")); });
    aspectMenu->addAction(QStringLiteral("4:3"), [this]() { m_engine->setAspectRatio(QStringLiteral("4:3")); });
    aspectMenu->addAction(QStringLiteral("2.35:1"), [this]() { m_engine->setAspectRatio(QStringLiteral("2.35:1")); });

    auto *speedMenu = m_contextMenu->addMenu(QStringLiteral("Playback Speed"));
    speedMenu->addAction(QStringLiteral("Normal (1.0x)\tZ"), [this]() { m_engine->setSpeed(1.0); });
    speedMenu->addAction(QStringLiteral("Faster (+0.1x)\tC"), [this]() { m_engine->setSpeed(m_engine->speed() + 0.1); });
    speedMenu->addAction(QStringLiteral("Slower (-0.1x)\tX"), [this]() { m_engine->setSpeed(m_engine->speed() - 0.1); });

    m_contextMenu->addSeparator();
    auto *closeAct = m_contextMenu->addAction(QStringLiteral("Exit\tCtrl+Q"), this, &QWidget::close);
}

void MainWindow::openMediaFile(const QString &filePath) {
    if (filePath.isEmpty()) return;
    m_engine->open(filePath);
}

void MainWindow::promptOpenFile() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Media File"),
        QString(),
        QStringLiteral("Media Files (*.mp4 *.mkv *.avi *.webm *.flv *.ts *.mp3 *.flac *.aac *.ogg *.opus *.wav);;All Files (*)")
    );
    if (!file.isEmpty()) {
        openMediaFile(file);
    }
}

void MainWindow::promptOpenUrl() {
    bool ok = false;
    const QString url = QInputDialog::getText(
        this,
        QStringLiteral("Open URL"),
        QStringLiteral("Enter stream or media URL:"),
        QLineEdit::Normal,
        QString(),
        &ok
    );
    if (ok && !url.trimmed().isEmpty()) {
        openMediaFile(url.trimmed());
    }
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::showContextMenu(const QPoint &globalPos) {
    if (m_contextMenu) {
        m_contextMenu->popup(globalPos);
    }
}

void MainWindow::toggleTimeDisplayMode() {
    m_showRemainingTime = !m_showRemainingTime;
    onPositionChanged(m_currentPosition);
}

void MainWindow::onStateChanged(PlaybackState state) {
    switch (state) {
    case PlaybackState::Playing:
        m_playPauseBtn->setText(QStringLiteral("⏸"));
        m_playPauseBtn->setToolTip(QStringLiteral("Pause (Space)"));
        break;
    case PlaybackState::Paused:
    case PlaybackState::Stopped:
    case PlaybackState::Idle:
    case PlaybackState::Error:
        m_playPauseBtn->setText(QStringLiteral("▶"));
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
    if (m_volumeSlider->value() != volume) {
        m_volumeSlider->blockSignals(true);
        m_volumeSlider->setValue(volume);
        m_volumeSlider->blockSignals(false);
    }
}

void MainWindow::onMuteChanged(bool muted) {
    m_muteBtn->setText(muted ? QStringLiteral("🔇") : QStringLiteral("🔊"));
}

void MainWindow::onSpeedChanged(double speed) {
    // Will be reflected in OSD / title
}

void MainWindow::onMetadataChanged(const MediaMetadata &metadata) {
    if (!metadata.title.isEmpty()) {
        setWindowTitle(QStringLiteral("%1 - PotPlayer Linux").arg(metadata.title));
    }
    updateBadges(metadata);
}

void MainWindow::updateBadges(const MediaMetadata &meta) {
    m_hwSwBadge->setText(meta.isHardwareDecoded ? QStringLiteral("H/W") : QStringLiteral("S/W"));
    m_hwSwBadge->setStyleSheet(meta.isHardwareDecoded
        ? QStringLiteral("QLabel#badge { background-color: #1A3820; border: 1px solid #287030; color: #44DD66; }")
        : QStringLiteral("QLabel#badge { background-color: #202020; border: 1px solid #383838; color: #AAAAAA; }"));

    if (meta.isHdr) {
        m_hdrBadge->show();
    } else {
        m_hdrBadge->hide();
    }

    if (!meta.videoCodec.isEmpty()) {
        m_videoCodecBadge->setText(meta.videoCodec);
        m_videoCodecBadge->show();
    } else {
        m_videoCodecBadge->hide();
    }

    if (!meta.audioCodec.isEmpty()) {
        m_audioCodecBadge->setText(meta.audioCodec);
        m_audioCodecBadge->show();
    } else {
        m_audioCodecBadge->hide();
    }

    if (meta.audioChannels > 0) {
        m_channelsBadge->setText(meta.audioChannels == 6 ? QStringLiteral("5.1")
                               : (meta.audioChannels == 8 ? QStringLiteral("7.1")
                               : QStringLiteral("2.0")));
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
    case Qt::Key_Up:
        m_engine->setVolume(m_engine->volume() + 2);
        event->accept();
        return;
    case Qt::Key_Down:
        m_engine->setVolume(m_engine->volume() - 2);
        event->accept();
        return;
    case Qt::Key_M:
        m_engine->toggleMute();
        event->accept();
        return;
    case Qt::Key_Z:
        m_engine->setSpeed(1.0);
        event->accept();
        return;
    case Qt::Key_X:
        m_engine->setSpeed(m_engine->speed() - 0.1);
        event->accept();
        return;
    case Qt::Key_C:
        m_engine->setSpeed(m_engine->speed() + 0.1);
        event->accept();
        return;
    case Qt::Key_D:
        m_engine->frameBackStep();
        event->accept();
        return;
    case Qt::Key_F:
        m_engine->frameStep();
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
        QString localPath = urls.first().toLocalFile();
        if (!localPath.isEmpty()) {
            openMediaFile(localPath);
        } else {
            openMediaFile(urls.first().toString());
        }
        event->acceptProposedAction();
    }
}

} // namespace PotPlayer
