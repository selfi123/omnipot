#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>

namespace PotPlayer {

class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override = default;

    void setTitle(const QString &title);
    void setMaximized(bool max);
    void setStayOnTop(bool onTop);

    QPushButton* menuButton() const { return m_menuBtn; }
    QPushButton* pinButton() const { return m_pinBtn; }
    QPushButton* minimizeButton() const { return m_minBtn; }
    QPushButton* maximizeButton() const { return m_maxBtn; }
    QPushButton* closeButton() const { return m_closeBtn; }

Q_SIGNALS:
    void menuRequested(const QPoint &globalPos);
    void pinToggled(bool onTop);
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void setupUi();

    QPushButton *m_menuBtn = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_pinBtn = nullptr;
    QPushButton *m_minBtn = nullptr;
    QPushButton *m_maxBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;

    bool m_isPinned = false;
};

} // namespace PotPlayer

