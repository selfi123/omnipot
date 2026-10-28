#include "ui/time_bar.h"
#include <QPainter>
#include <QColor>
#include <QStyleOptionSlider>
#include <algorithm>

namespace PotPlayer {

TimeBar::TimeBar(QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
{
    setRange(0, 10000);
    setValue(0);
    setMouseTracking(true);
    setFixedHeight(16);
    setCursor(Qt::PointingHandCursor);
}

void TimeBar::setDuration(double seconds) {
    m_duration = std::max(0.0, seconds);
    update();
}

void TimeBar::setPosition(double seconds) {
    m_position = std::clamp(seconds, 0.0, m_duration > 0.0 ? m_duration : 1.0);
    if (!isSliderDown() && m_duration > 0.0) {
        int val = static_cast<int>((m_position / m_duration) * 10000.0);
        setValue(val);
    }
    update();
}

void TimeBar::setRepeatA(double seconds) {
    m_repeatA = seconds;
    update();
}

void TimeBar::setRepeatB(double seconds) {
    m_repeatB = seconds;
    update();
}

void TimeBar::clearRepeat() {
    m_repeatA = -1.0;
    m_repeatB = -1.0;
    update();
}

double TimeBar::posToSeconds(int pixelX) const {
    if (width() <= 0 || m_duration <= 0.0) return 0.0;
    double ratio = std::clamp(static_cast<double>(pixelX) / static_cast<double>(width()), 0.0, 1.0);
    return ratio * m_duration;
}

QString TimeBar::formatTime(double seconds) const {
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

void TimeBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        double targetSec = posToSeconds(event->pos().x());
        setPosition(targetSec);
        Q_EMIT seekRequested(targetSec);
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void TimeBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_duration > 0.0) {
        double hoverSec = posToSeconds(event->pos().x());
        QPoint tipPos = event->globalPosition().toPoint() - QPoint(25, 34);
        QString tipHtml = QStringLiteral("<span style='color: #FFFFFF; font-weight: bold;'>%1</span>").arg(formatTime(hoverSec));
        QToolTip::showText(tipPos, tipHtml, this);
    }
    QSlider::mouseMoveEvent(event);
}

void TimeBar::leaveEvent(QEvent *event) {
    QToolTip::hideText();
    QSlider::leaveEvent(event);
}

void TimeBar::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int barHeight = 4;
    const int barY = (height() - barHeight) / 2;
    const int w = width();

    // 1. Background Track (Dark Gray)
    painter.fillRect(0, barY, w, barHeight, QColor(45, 45, 45));

    if (m_duration > 0.0) {
        // 2. Played Progress (PotPlayer Red Accent)
        double progressRatio = std::clamp(m_position / m_duration, 0.0, 1.0);
        int playedW = static_cast<int>(progressRatio * w);
        painter.fillRect(0, barY, playedW, barHeight, QColor(229, 38, 38));

        // 3. A-B Repeat Markers
        if (m_repeatA >= 0.0) {
            int aX = static_cast<int>((m_repeatA / m_duration) * w);
            painter.setPen(QColor(0, 200, 255));
            painter.drawLine(aX, barY - 3, aX, barY + barHeight + 3);
        }
        if (m_repeatB >= 0.0) {
            int bX = static_cast<int>((m_repeatB / m_duration) * w);
            painter.setPen(QColor(0, 200, 255));
            painter.drawLine(bX, barY - 3, bX, barY + barHeight + 3);
        }

        // 4. Scrubbing Handle / Thumb (Small circular bead on progress edge)
        painter.setBrush(QColor(255, 255, 255));
        painter.setPen(QColor(180, 0, 0));
        painter.drawEllipse(QPoint(playedW, barY + barHeight / 2), 4, 4);
    }
}

} // namespace PotPlayer
