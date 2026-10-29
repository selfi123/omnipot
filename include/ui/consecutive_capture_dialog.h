#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include "video/frame_capture.h"

namespace Omnipot {

class ConsecutiveCaptureDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConsecutiveCaptureDialog(FrameCapture *capture, mpv_handle *mpv, QWidget *parent = nullptr);
    ~ConsecutiveCaptureDialog() override = default;

private Q_SLOTS:
    void browseDirectory();
    void toggleCapture();
    void onProgressChanged(int current, int total);
    void onFinished(int totalCaptured);
    void onError(const QString &errorMessage);

private:
    void setupUi();

    FrameCapture *m_capture = nullptr;
    mpv_handle *m_mpv = nullptr;

    QLineEdit *m_dirEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QSpinBox *m_countSpin = nullptr;
    QSpinBox *m_intervalSpin = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
};

} // namespace Omnipot
