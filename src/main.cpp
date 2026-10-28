#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDebug>
#include "app/single_instance.h"
#include "playback/mpv_engine.h"
#include "ui/main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PotPlayer Linux"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("PotPlayerNative"));

    // CLI Parser compatible with PotPlayer options
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native PotPlayer Media Player for Linux"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("content"), QStringLiteral("Media file, directory or URL to open"));

    QCommandLineOption seekOpt(QStringLiteral("seek"), QStringLiteral("Start playback from specified time (hh:mm:ss.ms or seconds)"), QStringLiteral("time"));
    parser.addOption(seekOpt);

    QCommandLineOption screenshotOpt(QStringLiteral("screenshot"), QStringLiteral("Capture window screenshot and exit"), QStringLiteral("path"));
    parser.addOption(screenshotOpt);

    parser.process(app);

    const QStringList posArgs = parser.positionalArguments();

    // Single-instance check
    PotPlayer::SingleInstanceController singleInstance;
    if (singleInstance.sendArgsToExistingInstance(posArgs)) {
        // Forwarded successfully to the existing instance; exit immediately
        return 0;
    }

    // Initialize Playback Engine
    auto engine = std::make_unique<PotPlayer::MpvEngine>();
    if (!engine->initialize()) {
        qCritical() << "Fatal: Failed to initialize libmpv playback engine.";
        return 1;
    }

    int ret = 0;
    {
        // Create Main Window
        PotPlayer::MainWindow mainWindow(engine.get());

        // Start single instance IPC server
        singleInstance.startServer(&mainWindow);

        // Connect VideoWidget with libmpv render context
        auto *videoWidget = mainWindow.findChild<PotPlayer::VideoWidget*>();
        if (videoWidget) {
            videoWidget->setMpvHandle(engine->rawMpvHandle());
        }

        mainWindow.show();

        if (parser.isSet(screenshotOpt)) {
            mainWindow.resize(960, 560);
            QCoreApplication::processEvents();
            QPixmap pixmap = mainWindow.grab();
            pixmap.save(parser.value(screenshotOpt));
            QTimer::singleShot(50, &app, &QCoreApplication::quit);
            ret = app.exec();
        } else {
            // Check positional arguments for media path
            if (!posArgs.isEmpty()) {
                const QString targetMedia = posArgs.first();
                if (targetMedia == QStringLiteral("-")) {
                    mainWindow.openMediaFile(QStringLiteral("fd://0"));
                } else if (QFileInfo::exists(targetMedia) || targetMedia.contains(QStringLiteral("://"))) {
                    mainWindow.openMediaFile(targetMedia);

                    if (parser.isSet(seekOpt)) {
                        const QString seekVal = parser.value(seekOpt);
                        bool ok = false;
                        double seconds = seekVal.toDouble(&ok);
                        if (ok) {
                            engine->seek(seconds, true);
                        }
                    }
                }
            }

            ret = app.exec();
        }
    } // mainWindow and VideoWidget are cleanly destroyed here before engine shutdown

    engine->shutdown();
    return ret;
}
