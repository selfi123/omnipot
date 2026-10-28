#include "ui/info_overlay.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>

namespace PotPlayer {

InfoOverlay::InfoOverlay(PlaybackEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);

    m_textLabel = new QLabel(this);
    m_textLabel->setStyleSheet(QStringLiteral(
        "color: #00E5FF; font-family: monospace; font-size: 11px; font-weight: bold; background: transparent;"
    ));
    m_textLabel->setTextFormat(Qt::RichText);
    layout->addWidget(m_textLabel);

    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &InfoOverlay::updateStats);

    hide();
}

void InfoOverlay::toggleVisible() {
    setVisible(!isVisible());
}

void InfoOverlay::showEvent(QShowEvent *) {
    updateStats();
    m_timer->start();
}

void InfoOverlay::hideEvent(QHideEvent *) {
    m_timer->stop();
}

void InfoOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    p.fillPath(path, QColor(16, 16, 16, 220));
    p.setPen(QPen(QColor(0, 229, 255, 120), 1));
    p.drawPath(path);
}

void InfoOverlay::updateStats() {
    if (!m_engine) return;

    StreamStats s = m_engine->streamStats();
    const MediaMetadata &m = m_engine->metadata();

    QString hwText = s.hwdec.isEmpty() || s.hwdec == QStringLiteral("no")
        ? QStringLiteral("<span style='color:#FFB300;'>Software (CPU)</span>")
        : QStringLiteral("<span style='color:#00E676;'>Hardware (%1)</span>").arg(s.hwdec.toUpper());

    auto formatTime = [](double sec) {
        int total = static_cast<int>(sec);
        int h = total / 3600;
        int min = (total % 3600) / 60;
        int s = total % 60;
        if (h > 0) return QStringLiteral("%1:%2:%3").arg(h, 2, 10, QLatin1Char('0')).arg(min, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
        return QStringLiteral("%1:%2").arg(min, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    };

    QString html = QStringLiteral(
        "<div style='line-height: 140%;'>"
        "<span style='color:#FFFFFF; font-size:12px;'>[ POTPLAYER STREAM DIAGNOSTICS ]</span><br>"
        "<span style='color:#888888;'>Title:</span> <span style='color:#FFFFFF;'>%1</span><br>"
        "<span style='color:#888888;'>Time:</span> <span style='color:#FFFFFF;'>%2 / %3</span> "
        "(Speed: %4x)<br>"
        "<hr style='border: 0; border-top: 1px solid #333333; margin: 4px 0;'>"
        "<span style='color:#FF80AB;'>[ Video Stream ]</span><br>"
        "&nbsp;&nbsp;Codec: <span style='color:#FFFFFF;'>%5</span><br>"
        "&nbsp;&nbsp;Resolution: <span style='color:#FFFFFF;'>%6 x %7</span><br>"
        "&nbsp;&nbsp;Frame Rate: <span style='color:#FFFFFF;'>%8 fps</span><br>"
        "&nbsp;&nbsp;Video Bitrate: <span style='color:#FFFFFF;'>%9 kbps</span><br>"
        "&nbsp;&nbsp;Decoder: %10<br>"
        "<hr style='border: 0; border-top: 1px solid #333333; margin: 4px 0;'>"
        "<span style='color:#B388FF;'>[ Audio Stream ]</span><br>"
        "&nbsp;&nbsp;Codec: <span style='color:#FFFFFF;'>%11</span><br>"
        "&nbsp;&nbsp;Channels: <span style='color:#FFFFFF;'>%12 ch</span><br>"
        "&nbsp;&nbsp;Sample Rate: <span style='color:#FFFFFF;'>%13 Hz</span><br>"
        "&nbsp;&nbsp;Audio Bitrate: <span style='color:#FFFFFF;'>%14 kbps</span><br>"
        "<hr style='border: 0; border-top: 1px solid #333333; margin: 4px 0;'>"
        "<span style='color:#69F0AE;'>[ Performance & Sync ]</span><br>"
        "&nbsp;&nbsp;Dropped Frames: <span style='color:#FFFFFF;'>%15</span><br>"
        "&nbsp;&nbsp;A-V Sync Diff: <span style='color:#FFFFFF;'>%+16.6f s</span>"
        "</div>"
    )
    .arg(m.title.isEmpty() ? m.filename : m.title)
    .arg(formatTime(m_engine->position()))
    .arg(formatTime(m_engine->duration()))
    .arg(m_engine->speed(), 0, 'f', 2)
    .arg(s.videoCodec.isEmpty() ? QStringLiteral("N/A") : s.videoCodec)
    .arg(s.videoWidth)
    .arg(s.videoHeight)
    .arg(s.fps, 0, 'f', 3)
    .arg(s.videoBitrate / 1000)
    .arg(hwText)
    .arg(s.audioCodec.isEmpty() ? QStringLiteral("N/A") : s.audioCodec)
    .arg(s.audioChannels)
    .arg(s.audioSampleRate)
    .arg(s.audioBitrate / 1000)
    .arg(s.droppedFrames)
    .arg(s.avSyncDiff);

    m_textLabel->setText(html);
    adjustSize();
}

} // namespace PotPlayer
