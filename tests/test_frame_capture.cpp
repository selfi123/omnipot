#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include "video/frame_capture.h"

class TestFrameCapture : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testDefaultDirectory() {
        QString dir = Omnipot::FrameCapture::defaultCaptureDirectory();
        QVERIFY(!dir.isEmpty());
        QVERIFY(dir.endsWith(QStringLiteral("Omnipot")));
    }

    void testNullHandleGracefulFailure() {
        Omnipot::FrameCapture capture;
        QSignalSpy errSpy(&capture, &Omnipot::FrameCapture::captureError);

        QString savedPath;
        bool res = capture.captureFrame(nullptr, QString(), QStringLiteral("png"), &savedPath);
        QCOMPARE(res, false);
        QVERIFY(savedPath.isEmpty());
        QCOMPARE(errSpy.count(), 1);

        bool clipRes = capture.copyFrameToClipboard(nullptr);
        QCOMPARE(clipRes, false);
        QCOMPARE(errSpy.count(), 2);

        bool consecRes = capture.startConsecutiveCapture(nullptr, QString(), QStringLiteral("png"), 10, 500);
        QCOMPARE(consecRes, false);
        QCOMPARE(capture.isConsecutiveCaptureRunning(), false);
    }

    void testConsecutiveCaptureState() {
        Omnipot::FrameCapture capture;
        QCOMPARE(capture.isConsecutiveCaptureRunning(), false);
        QCOMPARE(capture.consecutiveProgress(), 0);
        QCOMPARE(capture.consecutiveTotal(), 0);

        capture.stopConsecutiveCapture();
        QCOMPARE(capture.isConsecutiveCaptureRunning(), false);
    }
};

QTEST_MAIN(TestFrameCapture)
#include "test_frame_capture.moc"
