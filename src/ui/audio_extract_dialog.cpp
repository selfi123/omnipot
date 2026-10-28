#include "ui/audio_extract_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

namespace PotPlayer {

AudioExtractDialog::AudioExtractDialog(const QString &sourceMedia, QWidget *parent)
    : QDialog(parent)
    , m_sourceMedia(sourceMedia)
{
    setWindowTitle(QStringLiteral("Audio Stream Extraction (Alt+A)"));
    setFixedSize(520, 340);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Segoe UI', 'Ubuntu', sans-serif; font-size: 12px; }"
        "QGroupBox { color: #f0a020; font-weight: bold; border: 1px solid #333333; margin-top: 10px; padding-top: 15px; border-radius: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; }"
        "QLabel { color: #cccccc; }"
        "QLineEdit, QComboBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 4px; }"
        "QPushButton { background-color: #333333; color: #ffffff; border: 1px solid #444444; border-radius: 3px; padding: 5px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #444444; border-color: #f0a020; }"
        "QPushButton:pressed { background-color: #222222; }"
        "QProgressBar { border: 1px solid #3c3c3c; border-radius: 3px; background-color: #2b2b2b; text-align: center; color: #ffffff; font-weight: bold; }"
        "QProgressBar::chunk { background-color: #f0a020; border-radius: 2px; }"
    ));

    setupUi();
}

AudioExtractDialog::~AudioExtractDialog() {
    cancelExtraction();
}

void AudioExtractDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *settingsGroup = new QGroupBox(QStringLiteral("Extraction Parameters"), this);
    auto *gridLayout = new QGridLayout(settingsGroup);
    gridLayout->setSpacing(8);

    // Source media
    gridLayout->addWidget(new QLabel(QStringLiteral("Input Media:"), settingsGroup), 0, 0);
    m_sourceEdit = new QLineEdit(m_sourceMedia, settingsGroup);
    m_sourceEdit->setReadOnly(true);
    gridLayout->addWidget(m_sourceEdit, 0, 1, 1, 2);

    // Output format
    gridLayout->addWidget(new QLabel(QStringLiteral("Audio Format:"), settingsGroup), 1, 0);
    m_formatCombo = new QComboBox(settingsGroup);
    m_formatCombo->addItems({QStringLiteral("MP3"), QStringLiteral("AAC"), QStringLiteral("FLAC"), QStringLiteral("WAV"), QStringLiteral("OGG"), QStringLiteral("Opus")});
    connect(m_formatCombo, &QComboBox::currentTextChanged, this, &AudioExtractDialog::updateFormatExtension);
    gridLayout->addWidget(m_formatCombo, 1, 1, 1, 2);

    // Bitrate
    gridLayout->addWidget(new QLabel(QStringLiteral("Bitrate:"), settingsGroup), 2, 0);
    m_bitrateCombo = new QComboBox(settingsGroup);
    m_bitrateCombo->addItems({QStringLiteral("320 kbps (High Quality)"), QStringLiteral("256 kbps"), QStringLiteral("192 kbps (Standard)"), QStringLiteral("128 kbps"), QStringLiteral("Lossless")});
    gridLayout->addWidget(m_bitrateCombo, 2, 1, 1, 2);

    // Output file
    gridLayout->addWidget(new QLabel(QStringLiteral("Save To:"), settingsGroup), 3, 0);
    QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (musicDir.isEmpty()) musicDir = QDir::homePath();
    QString base = QFileInfo(m_sourceMedia).completeBaseName();
    if (base.isEmpty()) base = QStringLiteral("audio_track");
    QString defaultOutput = musicDir + QStringLiteral("/") + base + QStringLiteral(".mp3");
    m_outputEdit = new QLineEdit(defaultOutput, settingsGroup);
    gridLayout->addWidget(m_outputEdit, 3, 1);

    auto *browseBtn = new QPushButton(QStringLiteral("Browse..."), settingsGroup);
    connect(browseBtn, &QPushButton::clicked, this, &AudioExtractDialog::browseOutputFile);
    gridLayout->addWidget(browseBtn, 3, 2);

    mainLayout->addWidget(settingsGroup);

    // Progress
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(QStringLiteral("Ready to extract audio stream."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #aaaaaa;"));
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_extractBtn = new QPushButton(QStringLiteral("Extract Audio"), this);
    m_extractBtn->setStyleSheet(QStringLiteral("background-color: #2e6030; border-color: #408844;"));
    connect(m_extractBtn, &QPushButton::clicked, this, &AudioExtractDialog::startExtraction);
    btnLayout->addWidget(m_extractBtn);

    m_cancelBtn = new QPushButton(QStringLiteral("Close"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(btnLayout);
}

void AudioExtractDialog::updateFormatExtension(const QString &format) {
    QString current = m_outputEdit->text();
    QFileInfo fi(current);
    QString newPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".") + format.toLower());
    m_outputEdit->setText(newPath);

    if (format == QStringLiteral("FLAC") || format == QStringLiteral("WAV")) {
        m_bitrateCombo->setCurrentText(QStringLiteral("Lossless"));
    } else if (m_bitrateCombo->currentText() == QStringLiteral("Lossless")) {
        m_bitrateCombo->setCurrentIndex(0); // 320 kbps
    }
}

void AudioExtractDialog::browseOutputFile() {
    QString fmt = m_formatCombo->currentText().toLower();
    QString filter = QStringLiteral("%1 Audio (*.%2);;All Files (*.*)").arg(fmt.toUpper(), fmt);
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Select Output Audio File"), m_outputEdit->text(), filter);
    if (!path.isEmpty()) {
        m_outputEdit->setText(path);
    }
}

void AudioExtractDialog::startExtraction() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        return;
    }

    if (m_sourceMedia.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Error: No source media file specified."));
        return;
    }

    QString outputPath = m_outputEdit->text().trimmed();
    if (outputPath.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Error: Destination file path cannot be empty."));
        return;
    }

    QString format = m_formatCombo->currentText();
    QString bitrateText = m_bitrateCombo->currentText();
    QString bitrate = QStringLiteral("320k");
    if (bitrateText.contains(QStringLiteral("256"))) bitrate = QStringLiteral("256k");
    else if (bitrateText.contains(QStringLiteral("192"))) bitrate = QStringLiteral("192k");
    else if (bitrateText.contains(QStringLiteral("128"))) bitrate = QStringLiteral("128k");

    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-i") << m_sourceMedia
         << QStringLiteral("-vn");

    if (format == QStringLiteral("MP3")) {
        args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame") << QStringLiteral("-b:a") << bitrate;
    } else if (format == QStringLiteral("AAC")) {
        args << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a") << bitrate;
    } else if (format == QStringLiteral("FLAC")) {
        args << QStringLiteral("-c:a") << QStringLiteral("flac");
    } else if (format == QStringLiteral("WAV")) {
        args << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le");
    } else if (format == QStringLiteral("OGG")) {
        args << QStringLiteral("-c:a") << QStringLiteral("libvorbis") << QStringLiteral("-b:a") << bitrate;
    } else if (format == QStringLiteral("Opus")) {
        args << QStringLiteral("-c:a") << QStringLiteral("libopus") << QStringLiteral("-b:a") << bitrate;
    }

    args << outputPath;

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished, this, &AudioExtractDialog::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &AudioExtractDialog::onProcessError);
    }

    m_progressBar->setRange(0, 0); // Indeterminate progress
    m_statusLabel->setText(QStringLiteral("Extracting audio stream via FFmpeg..."));
    m_extractBtn->setEnabled(false);

    m_process->start(QStringLiteral("ffmpeg"), args);
}

void AudioExtractDialog::cancelExtraction() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void AudioExtractDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_progressBar->setRange(0, 100);
    m_extractBtn->setEnabled(true);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_progressBar->setValue(100);
        m_statusLabel->setText(QStringLiteral("Audio extracted successfully: ") + m_outputEdit->text());
    } else {
        m_progressBar->setValue(0);
        m_statusLabel->setText(QStringLiteral("FFmpeg extraction failed (exit code %1).").arg(exitCode));
    }
}

void AudioExtractDialog::onProcessError(QProcess::ProcessError error) {
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_extractBtn->setEnabled(true);

    if (error == QProcess::FailedToStart) {
        m_statusLabel->setText(QStringLiteral("FFmpeg executable not found in PATH. Please install ffmpeg."));
    } else {
        m_statusLabel->setText(QStringLiteral("Process error during extraction."));
    }
}

} // namespace PotPlayer
