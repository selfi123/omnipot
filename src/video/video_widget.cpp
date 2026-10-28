#include "video/video_widget.h"
#include <QOpenGLContext>
#include <QPainter>
#include <QColor>
#include <QTimer>
#include <QDateTime>
#include <QLinearGradient>
#include <QDebug>
#include <cmath>
#include <clocale>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

namespace PotPlayer {

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_bars(32, 0.05f)
    , m_peaks(32, 0.05f)
{
    setMouseTracking(true);

    m_visualizerTimer = new QTimer(this);
    m_visualizerTimer->setInterval(33); // ~30 fps
    connect(m_visualizerTimer, &QTimer::timeout, this, [this]() {
        if (!m_visualizerEnabled) return;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (size_t i = 0; i < m_bars.size(); ++i) {
            float phase = 0.005f * static_cast<float>(now) + static_cast<float>(i) * 0.45f;
            float target = 0.15f + 0.70f * std::abs(std::sin(phase));
            m_bars[i] = m_bars[i] * 0.65f + target * 0.35f;

            if (m_bars[i] > m_peaks[i]) {
                m_peaks[i] = m_bars[i];
            } else {
                m_peaks[i] = std::max(0.05f, m_peaks[i] - 0.02f);
            }
        }
        update();
    });
}

VideoWidget::~VideoWidget() {
    if (m_mpvGL) {
        makeCurrent();
        mpv_render_context_free(m_mpvGL);
        m_mpvGL = nullptr;
        doneCurrent();
    }
}

void VideoWidget::setMpvHandle(mpv_handle *mpv) {
    if (m_mpv == mpv) return;
    m_mpv = mpv;
    if (isValid() && m_mpv && !m_mpvGL) {
        makeCurrent();
        initMpvRenderContext();
        doneCurrent();
    }
}

int64_t VideoWidget::nativeWid() const {
    return 0;
}

void VideoWidget::initializeGL() {
    initMpvRenderContext();
}

void VideoWidget::initMpvRenderContext() {
    if (!m_mpv || m_mpvGL) return;

    std::setlocale(LC_NUMERIC, "C");

    mpv_opengl_init_params gl_params{
        [](void *, const char *name) -> void* {
            auto *ctx = QOpenGLContext::currentContext();
            return ctx ? reinterpret_cast<void*>(ctx->getProcAddress(QByteArray(name))) : nullptr;
        },
        nullptr
    };

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    int err = mpv_render_context_create(&m_mpvGL, m_mpv, params);
    if (err < 0) {
        qWarning() << "[VideoWidget] mpv_render_context_create failed:" << mpv_error_string(err);
        return;
    }

    mpv_render_context_set_update_callback(m_mpvGL, [](void *ctx) {
        auto *self = static_cast<VideoWidget*>(ctx);
        if (self) {
            QMetaObject::invokeMethod(self, "onFrameReady", Qt::QueuedConnection);
        }
    }, this);
}

void VideoWidget::onFrameReady() {
    update();
}

void VideoWidget::paintGL() {
    if (m_mpvGL) {
        GLint fboId = defaultFramebufferObject();
        int w = static_cast<int>(width() * devicePixelRatioF());
        int h = static_cast<int>(height() * devicePixelRatioF());
        mpv_opengl_fbo fbo{fboId, w, h, 0};
        int flip_y = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context_render(m_mpvGL, params);
    } else {
        glClearColor(0.04f, 0.04f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (m_visualizerEnabled) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setPen(QColor(80, 80, 80));
        QFont font = painter.font();
        font.setPointSize(14);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(rect().adjusted(0, -80, 0, -80), Qt::AlignCenter, QStringLiteral("PotPlayer Linux"));

        int numBars = static_cast<int>(m_bars.size());
        int totalWidth = static_cast<int>(width() * 0.7);
        int startX = (width() - totalWidth) / 2;
        int barWidth = (totalWidth / numBars) - 3;
        if (barWidth < 2) barWidth = 2;
        int maxHeight = static_cast<int>(height() * 0.4);
        int baseY = static_cast<int>(height() * 0.75);

        for (int i = 0; i < numBars; ++i) {
            int x = startX + i * (barWidth + 3);
            int barHeight = static_cast<int>(m_bars[i] * maxHeight);
            int y = baseY - barHeight;

            QLinearGradient grad(x, baseY, x, baseY - maxHeight);
            grad.setColorAt(0.0, QColor(0, 220, 100));
            grad.setColorAt(0.7, QColor(255, 200, 0));
            grad.setColorAt(1.0, QColor(255, 60, 60));

            painter.fillRect(x, y, barWidth, barHeight, grad);

            int peakY = baseY - static_cast<int>(m_peaks[i] * maxHeight);
            painter.fillRect(x, peakY - 2, barWidth, 2, QColor(255, 255, 255));
        }
    } else if (m_isIdle) {
        // Authentic PotPlayer center watermark (Image parity)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        int cx = width() / 2;
        int cy = height() / 2;

        // PotPlayer icon: dark circular disc with play triangle
        const int discRadius = 26;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(42, 42, 42, 190));
        painter.drawEllipse(QPoint(cx - 75, cy), discRadius, discRadius);

        // Play triangle inside circle
        painter.setBrush(QColor(20, 20, 20, 240));
        QPolygon triangle;
        triangle << QPoint(cx - 80, cy - 10)
                 << QPoint(cx - 80, cy + 10)
                 << QPoint(cx - 66, cy);
        painter.drawPolygon(triangle);

        // Subtle grey "PotPlayer" typography
        QFont font = painter.font();
        font.setFamily(QStringLiteral("Segoe UI, Ubuntu, sans-serif"));
        font.setPointSize(23);
        font.setWeight(QFont::Medium);
        painter.setFont(font);
        painter.setPen(QColor(75, 75, 75, 220));
        painter.drawText(QRect(cx - 36, cy - 25, 200, 50), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("PotPlayer"));
    }
}

void VideoWidget::resizeGL(int /*w*/, int /*h*/) {
    update();
}

void VideoWidget::setIdle(bool idle) {
    if (m_isIdle != idle) {
        m_isIdle = idle;
        update();
    }
}

void VideoWidget::setVisualizerEnabled(bool enable) {
    m_visualizerEnabled = enable;
    if (enable) {
        m_visualizerTimer->start();
    } else {
        m_visualizerTimer->stop();
    }
    update();
}

void VideoWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT clicked();
    } else if (event->button() == Qt::RightButton) {
        Q_EMIT rightClicked(event->globalPosition().toPoint());
    } else if (event->button() == Qt::MiddleButton) {
        Q_EMIT middleClicked();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT doubleClicked();
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void VideoWidget::wheelEvent(QWheelEvent *event) {
    Q_EMIT wheelScrolled(event->angleDelta().y());
    event->accept();
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event) {
    Q_EMIT mouseMoved();
    QOpenGLWidget::mouseMoveEvent(event);
}

} // namespace PotPlayer
