#include "video/frame_capture.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QImage>
#include <QGuiApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDebug>

namespace PotPlayer {

FrameCapture::FrameCapture(QObject *parent)
    : QObject(parent)
{
    m_consecutiveTimer = new QTimer(this);
    connect(m_consecutiveTimer, &QTimer::timeout, this, &FrameCapture::onConsecutiveTimer);
}

FrameCapture::~FrameCapture() {
    stopConsecutiveCapture();
}

QString FrameCapture::defaultCaptureDirectory() {
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty()) {
        pictures = QDir::homePath() + QStringLiteral("/Pictures");
    }
    return pictures + QStringLiteral("/PotPlayer");
}

bool FrameCapture::captureFrame(mpv_handle *mpv, const QString &customDir, const QString &format, QString *savedPath) {
    if (!mpv) {
        emit captureError(QStringLiteral("MPV engine is not initialized."));
        return false;
    }

    QString dirPath = customDir.trimmed().isEmpty() ? defaultCaptureDirectory() : customDir;
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            emit captureError(QStringLiteral("Failed to create capture directory: ") + dirPath);
            return false;
        }
    }

    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    QString ext = format.trimmed().toLower();
    if (ext.isEmpty()) ext = QStringLiteral("png");
    QString filename = QStringLiteral("PotPlayer_%1.%2").arg(timestamp, ext);
    QString fullPath = dir.absoluteFilePath(filename);

    const char *cmd[] = {
        "screenshot-to-file",
        fullPath.toUtf8().constData(),
        "video",
        nullptr
    };

    int status = mpv_command(mpv, cmd);
    if (status < 0) {
        QString err = QStringLiteral("Screenshot failed: ") + QString::fromUtf8(mpv_error_string(status));
        emit captureError(err);
        return false;
    }

    if (savedPath) {
        *savedPath = fullPath;
    }

    emit frameCaptured(fullPath);
    return true;
}

bool FrameCapture::copyFrameToClipboard(mpv_handle *mpv) {
    if (!mpv) {
        emit captureError(QStringLiteral("MPV engine is not initialized."));
        return false;
    }

    QString tempFile = QDir::tempPath() + QStringLiteral("/potplayer_clip_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    const char *cmd[] = {
        "screenshot-to-file",
        tempFile.toUtf8().constData(),
        "video",
        nullptr
    };

    int status = mpv_command(mpv, cmd);
    if (status < 0) {
        emit captureError(QStringLiteral("Clipboard frame grab failed: ") + QString::fromUtf8(mpv_error_string(status)));
        return false;
    }

    if (QFile::exists(tempFile)) {
        QImage image(tempFile);
        if (!image.isNull()) {
            QGuiApplication::clipboard()->setImage(image);
            QFile::remove(tempFile);
            emit frameCopiedToClipboard();
            return true;
        }
        QFile::remove(tempFile);
    }

    emit captureError(QStringLiteral("Failed to read captured frame into image clipboard."));
    return false;
}

bool FrameCapture::startConsecutiveCapture(mpv_handle *mpv, const QString &targetDir, const QString &format, int totalCount, int intervalMs) {
    if (!mpv || totalCount <= 0 || intervalMs <= 0) {
        return false;
    }

    stopConsecutiveCapture();

    m_currentMpv = mpv;
    m_consecutiveDir = targetDir.trimmed().isEmpty() ? defaultCaptureDirectory() + QStringLiteral("/Consecutive") : targetDir;
    m_consecutiveFormat = format;
    m_totalToCapture = totalCount;
    m_capturedCount = 0;

    QDir dir(m_consecutiveDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    m_consecutiveTimer->setInterval(intervalMs);
    m_consecutiveTimer->start();

    // Take the first capture immediately
    onConsecutiveTimer();
    return true;
}

void FrameCapture::stopConsecutiveCapture() {
    if (m_consecutiveTimer->isActive()) {
        m_consecutiveTimer->stop();
        emit consecutiveFinished(m_capturedCount);
    }
    m_currentMpv = nullptr;
}

bool FrameCapture::isConsecutiveCaptureRunning() const {
    return m_consecutiveTimer && m_consecutiveTimer->isActive();
}

void FrameCapture::onConsecutiveTimer() {
    if (!m_currentMpv || m_capturedCount >= m_totalToCapture) {
        stopConsecutiveCapture();
        return;
    }

    QString ext = m_consecutiveFormat.trimmed().toLower();
    if (ext.isEmpty()) ext = QStringLiteral("png");

    QString numStr = QStringLiteral("%1").arg(m_capturedCount + 1, 4, 10, QLatin1Char('0'));
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    QString filename = QStringLiteral("Seq_%1_%2.%3").arg(timestamp, numStr, ext);
    QString fullPath = QDir(m_consecutiveDir).absoluteFilePath(filename);

    const char *cmd[] = {
        "screenshot-to-file",
        fullPath.toUtf8().constData(),
        "video",
        nullptr
    };

    int status = mpv_command(m_currentMpv, cmd);
    if (status >= 0) {
        m_capturedCount++;
        emit consecutiveProgressChanged(m_capturedCount, m_totalToCapture);
        emit frameCaptured(fullPath);
    }

    if (m_capturedCount >= m_totalToCapture) {
        stopConsecutiveCapture();
    }
}

} // namespace PotPlayer
