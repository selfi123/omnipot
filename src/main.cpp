#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDebug>
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

    parser.process(app);

    // Initialize Playback Engine
    auto engine = std::make_unique<PotPlayer::MpvEngine>();
    if (!engine->initialize()) {
        qCritical() << "Fatal: Failed to initialize libmpv playback engine.";
        return 1;
    }

    // Create Main Window
    PotPlayer::MainWindow mainWindow(engine.get());

    // Connect VideoWidget WID with libmpv
    auto *videoWidget = mainWindow.findChild<PotPlayer::VideoWidget*>();
    if (videoWidget) {
        engine->setWid(videoWidget->nativeWid());
    }

    mainWindow.show();

    // Check positional arguments for media path
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        const QString targetMedia = args.first();
        if (QFileInfo::exists(targetMedia) || targetMedia.contains(QStringLiteral("://"))) {
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

    int ret = app.exec();

    engine->shutdown();
    return ret;
}
