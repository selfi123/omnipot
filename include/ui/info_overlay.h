#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include "playback/playback_engine.h"

namespace Omnipot {

class InfoOverlay : public QWidget {
    Q_OBJECT

public:
    explicit InfoOverlay(PlaybackEngine *engine, QWidget *parent = nullptr);
    ~InfoOverlay() override = default;

    void toggleVisible();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private Q_SLOTS:
    void updateStats();

private:
    PlaybackEngine *m_engine = nullptr;
    QLabel *m_textLabel = nullptr;
    QTimer *m_timer = nullptr;
};

} // namespace Omnipot
