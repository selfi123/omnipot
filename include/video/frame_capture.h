#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <mpv/client.h>

namespace PotPlayer {

class FrameCapture : public QObject {
    Q_OBJECT

public:
    explicit FrameCapture(QObject *parent = nullptr);
    ~FrameCapture() override;

    static QString defaultCaptureDirectory();

    bool captureFrame(mpv_handle *mpv, const QString &customDir = QString(), const QString &format = QStringLiteral("png"), QString *savedPath = nullptr);
    bool copyFrameToClipboard(mpv_handle *mpv);

    bool startConsecutiveCapture(mpv_handle *mpv, const QString &targetDir, const QString &format, int totalCount, int intervalMs);
    void stopConsecutiveCapture();
    bool isConsecutiveCaptureRunning() const;
    int consecutiveProgress() const { return m_capturedCount; }
    int consecutiveTotal() const { return m_totalToCapture; }

Q_SIGNALS:
    void frameCaptured(const QString &filePath);
    void frameCopiedToClipboard();
    void consecutiveProgressChanged(int current, int total);
    void consecutiveFinished(int totalCaptured);
    void captureError(const QString &errorMessage);

private Q_SLOTS:
    void onConsecutiveTimer();

private:
    mpv_handle *m_currentMpv = nullptr;
    QString m_consecutiveDir;
    QString m_consecutiveFormat;
    int m_totalToCapture = 0;
    int m_capturedCount = 0;
    QTimer *m_consecutiveTimer = nullptr;
};

} // namespace PotPlayer
