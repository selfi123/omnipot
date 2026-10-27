#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QWheelEvent>

namespace PotPlayer {

class VideoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override = default;

    int64_t nativeWid() const;

Q_SIGNALS:
    void clicked();
    void doubleClicked();
    void rightClicked(const QPoint &globalPos);
    void wheelScrolled(int delta);
    void mouseMoved();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

} // namespace PotPlayer
