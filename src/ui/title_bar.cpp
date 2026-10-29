#include "ui/title_bar.h"
#include "ui/omni_icons.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>

namespace Omnipot {

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TitleBar::setupUi() {
    setFixedHeight(26);
    setStyleSheet(QStringLiteral(
        "QWidget#titleBar { background-color: #1a1a1a; border-bottom: 1px solid #262626; }"
        "QPushButton#titleMenuBtn { background-color: transparent; border: none; color: #CCCCCC; font-size: 11px; font-weight: bold; padding: 2px 8px; }"
        "QPushButton#titleMenuBtn:hover { background-color: #2b2b2b; color: #FFFFFF; }"
        "QPushButton#titleMenuBtn:pressed { background-color: #141414; }"
        "QLabel#titleLabel { color: #888888; font-size: 11px; padding-left: 6px; }"
        "QPushButton#titleWinBtn { background-color: transparent; border: none; color: #AAAAAA; font-size: 11px; width: 32px; min-height: 25px; }"
        "QPushButton#titleWinBtn:hover { background-color: #2e2e2e; color: #FFFFFF; }"
        "QPushButton#titleWinBtn:pressed { background-color: #181818; }"
        "QPushButton#titleWinBtn:checked { color: #F0A020; }"
        "QPushButton#titleCloseBtn { background-color: transparent; border: none; color: #AAAAAA; font-size: 12px; width: 34px; min-height: 25px; }"
        "QPushButton#titleCloseBtn:hover { background-color: #E81123; color: #FFFFFF; }"
        "QPushButton#titleCloseBtn:pressed { background-color: #BF0F1D; color: #FFFFFF; }"
    ));
    setObjectName(QStringLiteral("titleBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 0, 0);
    layout->setSpacing(0);

    // Left: "Omnipot ▾" main menu button
    m_menuBtn = new QPushButton(QStringLiteral("Omnipot ▾"), this);
    m_menuBtn->setObjectName(QStringLiteral("titleMenuBtn"));
    m_menuBtn->setFocusPolicy(Qt::NoFocus);
    m_menuBtn->setToolTip(QStringLiteral("Main Menu (F1 / Right Click)"));
    connect(m_menuBtn, &QPushButton::clicked, [this]() {
        Q_EMIT menuRequested(m_menuBtn->mapToGlobal(QPoint(0, m_menuBtn->height())));
    });
    layout->addWidget(m_menuBtn);

    // Center: Draggable Title Label
    m_titleLabel = new QLabel(QStringLiteral("Omnipot"), this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setTextFormat(Qt::PlainText); // titles come from media metadata
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_titleLabel, 1);

    // Right: Window action buttons
    m_pinBtn = new QPushButton(this);
    m_pinBtn->setObjectName(QStringLiteral("titleWinBtn"));
    m_pinBtn->setIcon(OmniIcons::pinIcon(m_isPinned, 14));
    m_pinBtn->setIconSize(QSize(14, 14));
    m_pinBtn->setCheckable(true);
    m_pinBtn->setFocusPolicy(Qt::NoFocus);
    m_pinBtn->setToolTip(QStringLiteral("Always on Top"));
    connect(m_pinBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_isPinned = checked;
        m_pinBtn->setIcon(OmniIcons::pinIcon(checked, 14));
        Q_EMIT pinToggled(checked);
    });
    layout->addWidget(m_pinBtn);

    m_minBtn = new QPushButton(QStringLiteral("─"), this);
    m_minBtn->setObjectName(QStringLiteral("titleWinBtn"));
    m_minBtn->setFocusPolicy(Qt::NoFocus);
    m_minBtn->setToolTip(QStringLiteral("Minimize"));
    connect(m_minBtn, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    layout->addWidget(m_minBtn);

    m_maxBtn = new QPushButton(QStringLiteral("🗖"), this);
    m_maxBtn->setObjectName(QStringLiteral("titleWinBtn"));
    m_maxBtn->setFocusPolicy(Qt::NoFocus);
    m_maxBtn->setToolTip(QStringLiteral("Maximize"));
    connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::maximizeRequested);
    layout->addWidget(m_maxBtn);

    m_closeBtn = new QPushButton(QStringLiteral("✕"), this);
    m_closeBtn->setObjectName(QStringLiteral("titleCloseBtn"));
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setToolTip(QStringLiteral("Close"));
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeRequested);
    layout->addWidget(m_closeBtn);
}

void TitleBar::setTitle(const QString &title) {
    if (m_titleLabel) {
        m_titleLabel->setText(title.isEmpty() ? QStringLiteral("Omnipot") : title);
    }
}

void TitleBar::setMaximized(bool max) {
    if (m_maxBtn) {
        m_maxBtn->setText(max ? QStringLiteral("🗗") : QStringLiteral("🗖"));
        m_maxBtn->setToolTip(max ? QStringLiteral("Restore") : QStringLiteral("Maximize"));
    }
}

void TitleBar::setStayOnTop(bool onTop) {
    m_isPinned = onTop;
    if (m_pinBtn) {
        m_pinBtn->blockSignals(true);
        m_pinBtn->setChecked(onTop);
        m_pinBtn->setIcon(OmniIcons::pinIcon(onTop, 14));
        m_pinBtn->blockSignals(false);
    }
}

void TitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (window() && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
    } else if (event->button() == Qt::RightButton) {
        Q_EMIT menuRequested(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        Q_EMIT doubleClicked();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace Omnipot

