#pragma once

#include <QSlider>
#include <QMouseEvent>
#include <QToolTip>

namespace PotPlayer {

class TimeBar : public QSlider {
    Q_OBJECT

public:
    explicit TimeBar(QWidget *parent = nullptr);
    ~TimeBar() override = default;

    void setDuration(double seconds);
    void setPosition(double seconds);

    void setRepeatA(double seconds);
    void setRepeatB(double seconds);
    void clearRepeat();

Q_SIGNALS:
    void seekRequested(double seconds);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    double posToSeconds(int pixelX) const;
    QString formatTime(double seconds) const;

    double m_duration = 0.0;
    double m_position = 0.0;
    double m_repeatA = -1.0;
    double m_repeatB = -1.0;
    bool m_isHovered = false;
};

} // namespace PotPlayer
