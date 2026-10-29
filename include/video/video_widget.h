#pragma once

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <vector>

struct mpv_handle;
struct mpv_render_context;

namespace Omnipot {

class VideoWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override;

    void setMpvHandle(mpv_handle *mpv);
    int64_t nativeWid() const;

    void setVisualizerEnabled(bool enable);
    bool isVisualizerEnabled() const { return m_visualizerEnabled; }

    void setIdle(bool idle);
    bool isIdle() const { return m_isIdle; }

public Q_SLOTS:
    void onFrameReady();

Q_SIGNALS:
    void clicked();
    void doubleClicked();
    void rightClicked(const QPoint &globalPos);
    void middleClicked();
    void wheelScrolled(int delta);
    void mouseMoved();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void initMpvRenderContext();

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_mpvGL = nullptr;

    bool m_visualizerEnabled = false;
    bool m_isIdle = true;
    class QTimer *m_visualizerTimer = nullptr;
    std::vector<float> m_bars;
    std::vector<float> m_peaks;
};

} // namespace Omnipot
