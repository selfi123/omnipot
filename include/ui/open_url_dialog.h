#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace PotPlayer {

class OpenUrlDialog : public QDialog {
    Q_OBJECT

public:
    explicit OpenUrlDialog(QWidget *parent = nullptr);
    ~OpenUrlDialog() override = default;

    QString url() const;
    QString userAgent() const;
    QString referrer() const;

private:
    void setupUi();

    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_userAgentEdit = nullptr;
    QLineEdit *m_referrerEdit = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

} // namespace PotPlayer
