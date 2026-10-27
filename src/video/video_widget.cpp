#include "video/video_widget.h"
#include <QPainter>
#include <QColor>

namespace PotPlayer {

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    // Native window flags for optimal embedding
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setMouseTracking(true);

    // Dark background matching PotPlayer
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(10, 10, 10));
    setPalette(pal);
    setAutoFillBackground(true);
}

int64_t VideoWidget::nativeWid() const {
    return static_cast<int64_t>(winId());
}

void VideoWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT clicked();
    } else if (event->button() == Qt::RightButton) {
        Q_EMIT rightClicked(event->globalPosition().toPoint());
    }
    QWidget::mousePressEvent(event);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT doubleClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void VideoWidget::wheelEvent(QWheelEvent *event) {
    Q_EMIT wheelScrolled(event->angleDelta().y());
    event->accept();
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event) {
    Q_EMIT mouseMoved();
    QWidget::mouseMoveEvent(event);
}

void VideoWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 10));
}

} // namespace PotPlayer
