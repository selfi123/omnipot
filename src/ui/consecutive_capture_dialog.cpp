#include "ui/consecutive_capture_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QGroupBox>

namespace Omnipot {

ConsecutiveCaptureDialog::ConsecutiveCaptureDialog(FrameCapture *capture, mpv_handle *mpv, QWidget *parent)
    : QDialog(parent)
    , m_capture(capture)
    , m_mpv(mpv)
{
    setWindowTitle(QStringLiteral("Consecutive Image Capture (Ctrl+Alt+E)"));
    setFixedSize(480, 320);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Segoe UI', 'Ubuntu', sans-serif; font-size: 12px; }"
        "QGroupBox { color: #f0a020; font-weight: bold; border: 1px solid #333333; margin-top: 10px; padding-top: 15px; border-radius: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; }"
        "QLabel { color: #cccccc; }"
        "QLineEdit, QComboBox, QSpinBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 4px; }"
        "QPushButton { background-color: #333333; color: #ffffff; border: 1px solid #444444; border-radius: 3px; padding: 5px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #444444; border-color: #f0a020; }"
        "QPushButton:pressed { background-color: #222222; }"
        "QProgressBar { border: 1px solid #3c3c3c; border-radius: 3px; background-color: #2b2b2b; text-align: center; color: #ffffff; font-weight: bold; }"
        "QProgressBar::chunk { background-color: #f0a020; border-radius: 2px; }"
    ));

    setupUi();

    connect(m_capture, &FrameCapture::consecutiveProgressChanged, this, &ConsecutiveCaptureDialog::onProgressChanged);
    connect(m_capture, &FrameCapture::consecutiveFinished, this, &ConsecutiveCaptureDialog::onFinished);
    connect(m_capture, &FrameCapture::captureError, this, &ConsecutiveCaptureDialog::onError);
}

void ConsecutiveCaptureDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *settingsGroup = new QGroupBox(QStringLiteral("Capture Settings"), this);
    auto *gridLayout = new QGridLayout(settingsGroup);
    gridLayout->setSpacing(8);

    // Directory
    gridLayout->addWidget(new QLabel(QStringLiteral("Destination:"), settingsGroup), 0, 0);
    m_dirEdit = new QLineEdit(FrameCapture::defaultCaptureDirectory() + QStringLiteral("/Consecutive"), settingsGroup);
    gridLayout->addWidget(m_dirEdit, 0, 1);
    auto *browseBtn = new QPushButton(QStringLiteral("Browse..."), settingsGroup);
    connect(browseBtn, &QPushButton::clicked, this, &ConsecutiveCaptureDialog::browseDirectory);
    gridLayout->addWidget(browseBtn, 0, 2);

    // Format
    gridLayout->addWidget(new QLabel(QStringLiteral("Format:"), settingsGroup), 1, 0);
    m_formatCombo = new QComboBox(settingsGroup);
    m_formatCombo->addItems({QStringLiteral("PNG"), QStringLiteral("JPG"), QStringLiteral("WebP")});
    gridLayout->addWidget(m_formatCombo, 1, 1, 1, 2);

    // Total Count
    gridLayout->addWidget(new QLabel(QStringLiteral("Total Frames:"), settingsGroup), 2, 0);
    m_countSpin = new QSpinBox(settingsGroup);
    m_countSpin->setRange(1, 1000);
    m_countSpin->setValue(20);
    gridLayout->addWidget(m_countSpin, 2, 1, 1, 2);

    // Interval (ms)
    gridLayout->addWidget(new QLabel(QStringLiteral("Interval (ms):"), settingsGroup), 3, 0);
    m_intervalSpin = new QSpinBox(settingsGroup);
    m_intervalSpin->setRange(50, 10000);
    m_intervalSpin->setSingleStep(50);
    m_intervalSpin->setValue(500);
    gridLayout->addWidget(m_intervalSpin, 3, 1, 1, 2);

    mainLayout->addWidget(settingsGroup);

    // Progress
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 20);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(QStringLiteral("Ready to capture"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #aaaaaa;"));
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_startBtn = new QPushButton(QStringLiteral("Start Capture"), this);
    m_startBtn->setStyleSheet(QStringLiteral("background-color: #2e6030; border-color: #408844;"));
    connect(m_startBtn, &QPushButton::clicked, this, &ConsecutiveCaptureDialog::toggleCapture);
    btnLayout->addWidget(m_startBtn);

    m_closeBtn = new QPushButton(QStringLiteral("Close"), this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(btnLayout);
}

void ConsecutiveCaptureDialog::browseDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Output Directory"), m_dirEdit->text());
    if (!dir.isEmpty()) {
        m_dirEdit->setText(dir);
    }
}

void ConsecutiveCaptureDialog::toggleCapture() {
    if (m_capture->isConsecutiveCaptureRunning()) {
        m_capture->stopConsecutiveCapture();
        m_startBtn->setText(QStringLiteral("Start Capture"));
        m_startBtn->setStyleSheet(QStringLiteral("background-color: #2e6030; border-color: #408844;"));
        m_statusLabel->setText(QStringLiteral("Capture stopped by user."));
    } else {
        m_progressBar->setRange(0, m_countSpin->value());
        m_progressBar->setValue(0);
        bool ok = m_capture->startConsecutiveCapture(
            m_mpv,
            m_dirEdit->text(),
            m_formatCombo->currentText(),
            m_countSpin->value(),
            m_intervalSpin->value()
        );
        if (ok) {
            m_startBtn->setText(QStringLiteral("Stop Capture"));
            m_startBtn->setStyleSheet(QStringLiteral("background-color: #882222; border-color: #aa3333;"));
            m_statusLabel->setText(QStringLiteral("Capturing frame sequences..."));
        } else {
            m_statusLabel->setText(QStringLiteral("Failed to initiate capture. Verify video is loaded."));
        }
    }
}

void ConsecutiveCaptureDialog::onProgressChanged(int current, int total) {
    m_progressBar->setValue(current);
    m_statusLabel->setText(QStringLiteral("Captured frame %1 of %2").arg(current).arg(total));
}

void ConsecutiveCaptureDialog::onFinished(int totalCaptured) {
    m_startBtn->setText(QStringLiteral("Start Capture"));
    m_startBtn->setStyleSheet(QStringLiteral("background-color: #2e6030; border-color: #408844;"));
    m_statusLabel->setText(QStringLiteral("Completed! Captured %1 frames.").arg(totalCaptured));
}

void ConsecutiveCaptureDialog::onError(const QString &errorMessage) {
    m_statusLabel->setText(QStringLiteral("Error: ") + errorMessage);
}

} // namespace Omnipot
