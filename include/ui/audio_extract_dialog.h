#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QProcess>

namespace Omnipot {

class AudioExtractDialog : public QDialog {
    Q_OBJECT

public:
    explicit AudioExtractDialog(const QString &sourceMedia, QWidget *parent = nullptr);
    ~AudioExtractDialog() override;

private Q_SLOTS:
    void browseOutputFile();
    void startExtraction();
    void cancelExtraction();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void updateFormatExtension(const QString &format);

private:
    void setupUi();

    QString m_sourceMedia;
    QProcess *m_process = nullptr;

    QLineEdit *m_sourceEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_bitrateCombo = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_extractBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

} // namespace Omnipot
