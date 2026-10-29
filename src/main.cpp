#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDebug>
#include <QPixmap>
#include <QTimer>
#include "app/single_instance.h"
#include "playback/mpv_engine.h"
#include "ui/main_window.h"

// Accepts plain seconds ("2520", "12.5") or [[hh:]mm:]ss[.ms] ("00:42:00"); returns -1 if invalid
static double parseSeekTime(const QString &value) {
    const QStringList parts = value.trimmed().split(QLatin1Char(':'));
    if (parts.isEmpty() || parts.size() > 3) return -1.0;
    double seconds = 0.0;
    for (const QString &part : parts) {
        bool ok = false;
        const double v = part.toDouble(&ok);
        if (!ok || v < 0.0) return -1.0;
        seconds = seconds * 60.0 + v;
    }
    return seconds;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Omnipot"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("Omnipot"));
    app.setDesktopFileName(QStringLiteral("omnipot"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omnipot")));

    // CLI Parser
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Omnipot - native media player for Linux"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("content"), QStringLiteral("Media file, directory or URL to open"));

    QCommandLineOption seekOpt(QStringLiteral("seek"), QStringLiteral("Start playback from specified time (hh:mm:ss.ms or seconds)"), QStringLiteral("time"));
    parser.addOption(seekOpt);

    QCommandLineOption screenshotOpt(QStringLiteral("screenshot"), QStringLiteral("Capture window screenshot and exit"), QStringLiteral("path"));
    parser.addOption(screenshotOpt);

    parser.process(app);

    // Resolve local paths here: a running instance has a different working directory
    QStringList mediaArgs;
    for (const QString &arg : parser.positionalArguments()) {
        if (arg == QStringLiteral("-") || arg.contains(QStringLiteral("://"))) {
            mediaArgs.append(arg);
        } else if (QFileInfo::exists(arg)) {
            mediaArgs.append(QFileInfo(arg).absoluteFilePath());
        } else {
            qWarning() << "Ignoring missing file:" << arg;
        }
    }
    const bool readsStdin = mediaArgs.contains(QStringLiteral("-"));

    // Single-instance check (stdin can only be played by this process, so never forward it)
    Omnipot::SingleInstanceController singleInstance;
    if (!readsStdin && singleInstance.sendArgsToExistingInstance(mediaArgs)) {
        // Forwarded successfully to the existing instance; exit immediately
        return 0;
    }

    // Initialize Playback Engine
    auto engine = std::make_unique<Omnipot::MpvEngine>();
    if (!engine->initialize()) {
        qCritical() << "Fatal: Failed to initialize libmpv playback engine.";
        return 1;
    }

    int ret = 0;
    {
        // Create Main Window
        Omnipot::MainWindow mainWindow(engine.get());

        // Start single instance IPC server
        singleInstance.startServer(&mainWindow);

        mainWindow.show();

        if (parser.isSet(screenshotOpt)) {
            mainWindow.resize(960, 560);
            QCoreApplication::processEvents();
            QPixmap pixmap = mainWindow.grab();
            pixmap.save(parser.value(screenshotOpt));
            QTimer::singleShot(50, &app, &QCoreApplication::quit);
            ret = app.exec();
        } else {
            double startSeconds = -1.0;
            if (parser.isSet(seekOpt)) {
                startSeconds = parseSeekTime(parser.value(seekOpt));
                if (startSeconds < 0.0) {
                    qWarning() << "Ignoring invalid --seek value:" << parser.value(seekOpt);
                }
            }

            QStringList paths;
            for (const QString &arg : std::as_const(mediaArgs)) {
                paths.append(arg == QStringLiteral("-") ? QStringLiteral("fd://0") : arg);
            }
            mainWindow.openPaths(paths, startSeconds);

            ret = app.exec();
        }
    } // mainWindow and VideoWidget are cleanly destroyed here before engine shutdown

    engine->shutdown();
    return ret;
}
