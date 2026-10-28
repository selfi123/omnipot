#include "ui/open_url_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

namespace PotPlayer {

OpenUrlDialog::OpenUrlDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Open URL / Network Stream (Ctrl+U)"));
    setFixedSize(500, 260);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Segoe UI', 'Ubuntu', sans-serif; font-size: 12px; }"
        "QGroupBox { color: #f0a020; font-weight: bold; border: 1px solid #333333; margin-top: 10px; padding-top: 15px; border-radius: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; }"
        "QLabel { color: #cccccc; }"
        "QLineEdit { background-color: #2b2b2b; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 5px; }"
        "QPushButton { background-color: #333333; color: #ffffff; border: 1px solid #444444; border-radius: 3px; padding: 5px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #444444; border-color: #f0a020; }"
        "QPushButton:pressed { background-color: #222222; }"
    ));

    setupUi();
}

void OpenUrlDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *group = new QGroupBox(QStringLiteral("Network Stream Location"), this);
    auto *gridLayout = new QGridLayout(group);
    gridLayout->setSpacing(8);

    gridLayout->addWidget(new QLabel(QStringLiteral("URL:"), group), 0, 0);
    m_urlEdit = new QLineEdit(group);
    m_urlEdit->setPlaceholderText(QStringLiteral("http://, https://, rtmp://, rtsp://, or HLS/DASH manifest"));
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
    gridLayout->addWidget(m_urlEdit, 0, 1);

    gridLayout->addWidget(new QLabel(QStringLiteral("User-Agent (opt):"), group), 1, 0);
    m_userAgentEdit = new QLineEdit(group);
    m_userAgentEdit->setPlaceholderText(QStringLiteral("Leave blank for default player User-Agent"));
    gridLayout->addWidget(m_userAgentEdit, 1, 1);

    gridLayout->addWidget(new QLabel(QStringLiteral("Referer (opt):"), group), 2, 0);
    m_referrerEdit = new QLineEdit(group);
    m_referrerEdit->setPlaceholderText(QStringLiteral("Custom HTTP referer header if required by host"));
    gridLayout->addWidget(m_referrerEdit, 2, 1);

    mainLayout->addWidget(group);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_openBtn = new QPushButton(QStringLiteral("Open Stream"), this);
    m_openBtn->setStyleSheet(QStringLiteral("background-color: #2e6030; border-color: #408844;"));
    connect(m_openBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_openBtn);

    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(btnLayout);
}

QString OpenUrlDialog::url() const {
    return m_urlEdit->text().trimmed();
}

QString OpenUrlDialog::userAgent() const {
    return m_userAgentEdit->text().trimmed();
}

QString OpenUrlDialog::referrer() const {
    return m_referrerEdit->text().trimmed();
}

} // namespace PotPlayer
