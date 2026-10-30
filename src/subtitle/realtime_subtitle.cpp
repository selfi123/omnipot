#include "subtitle/realtime_subtitle.h"
#include "subtitle/whisper_model.h"
#include "config/settings_manager.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <mpv/client.h>
#include <algorithm>

#ifndef OMNIPOT_HAVE_WHISPER
#define OMNIPOT_HAVE_WHISPER 0
#endif

#if OMNIPOT_HAVE_WHISPER
#include "subtitle/realtime_subtitle_worker.h"
#endif

namespace Omnipot {

// Longest the player waits for the first subtitles before playing anyway
static constexpr int kMaxHoldMs = 15000;

static int cpuThreads() {
    return std::max(1, QThread::idealThreadCount());
}

RealtimeSubtitleController::RealtimeSubtitleController(PlaybackEngine *engine, QWidget *dialogParent)
    : QObject(dialogParent)
    , m_engine(engine)
    , m_dialogParent(dialogParent)
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/omnipot");
    QDir().mkpath(cacheDir);
    m_srtPath = cacheDir + QStringLiteral("/realtime-subtitle.srt");

    if (isAvailable()) {
        const int stored = SettingsManager::instance().realtimeSubtitleMode();
        if (stored == static_cast<int>(RealtimeSubtitleMode::Original) || stored == static_cast<int>(RealtimeSubtitleMode::TranslateToEnglish)) {
            m_mode = static_cast<RealtimeSubtitleMode>(stored);
        }
    }

    connect(m_engine, &PlaybackEngine::stateChanged, this, &RealtimeSubtitleController::onStateChanged);
    connect(m_engine, &PlaybackEngine::tracksChanged, this, &RealtimeSubtitleController::onTracksChanged);
#if OMNIPOT_HAVE_WHISPER
    connect(m_engine, &PlaybackEngine::positionChanged, this, [this](double seconds) {
        if (m_worker) m_worker->setPosition(seconds);
    });
    connect(m_engine, &PlaybackEngine::durationChanged, this, [this](double seconds) {
        if (m_worker) m_worker->setDuration(seconds);
    });
#endif
}

RealtimeSubtitleController::~RealtimeSubtitleController() {
    stopWorker();
}

bool RealtimeSubtitleController::isAvailable() {
    return OMNIPOT_HAVE_WHISPER;
}

QString RealtimeSubtitleController::modelId() const {
    return WhisperModels::resolve(SettingsManager::instance().realtimeSubtitleModel(), cpuThreads());
}

void RealtimeSubtitleController::setMode(RealtimeSubtitleMode mode) {
    if (mode == m_mode || !isAvailable()) {
        Q_EMIT modeChanged(m_mode);
        return;
    }

    if (mode != RealtimeSubtitleMode::Off) {
        if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty()) {
            QMessageBox::warning(m_dialogParent, QStringLiteral("Realtime subtitle"),
                QStringLiteral("Realtime subtitle needs the ffmpeg program to read the audio.\n\n"
                               "Install it with your package manager (for example: sudo apt install ffmpeg) and try again."));
            Q_EMIT modeChanged(m_mode);
            return;
        }
        if (!ensureModel(modelId())) {
            Q_EMIT modeChanged(m_mode);
            return;
        }
    }

    m_mode = mode;
    SettingsManager::instance().setRealtimeSubtitleMode(static_cast<int>(mode));

    stopWorker();
    removeTrack();
    if (m_mode == RealtimeSubtitleMode::Off) {
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: Off"));
    } else if (m_mediaReady) {
        startWorker();
    } else {
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: On (starts when media plays)"));
    }
    Q_EMIT modeChanged(m_mode);
}

void RealtimeSubtitleController::mediaOpened(const QString &url) {
    stopWorker();
    // mpv drops external subtitle tracks when it loads another file
    m_trackAdded = false;
    m_mediaReady = false;
    m_mediaUrl = url;
    m_unsupportedNotice.clear();
    if (url.startsWith(QStringLiteral("fd://")) || url == QStringLiteral("-")) {
        m_unsupportedNotice = QStringLiteral("Realtime subtitle can't read audio from standard input");
    }
}

void RealtimeSubtitleController::reloadSettings() {
    if (m_mode == RealtimeSubtitleMode::Off) return;
    // "Reset to defaults" turns the feature off behind our back
    if (SettingsManager::instance().realtimeSubtitleMode() == static_cast<int>(RealtimeSubtitleMode::Off)) {
        setMode(RealtimeSubtitleMode::Off);
        return;
    }
    if (!ensureModel(modelId())) {
        setMode(RealtimeSubtitleMode::Off);
        return;
    }
    if (m_worker) {
        stopWorker();
        removeTrack();
        startWorker();
    }
}

void RealtimeSubtitleController::onStateChanged(PlaybackState state) {
    switch (state) {
    case PlaybackState::Playing:
    case PlaybackState::Paused:
        if (!m_mediaReady && !m_mediaUrl.isEmpty()) {
            m_mediaReady = true;
            startWorker(state == PlaybackState::Playing);
        } else if (state == PlaybackState::Playing && m_holdingPlayback) {
            // The user pressed play while we were waiting; don't fight them
            m_holdingPlayback = false;
        }
        break;
    case PlaybackState::Idle:
    case PlaybackState::Stopped:
    case PlaybackState::Error:
        stopWorker();
        m_trackAdded = false;
        m_mediaReady = false;
        break;
    default:
        break;
    }
}

void RealtimeSubtitleController::onTracksChanged() {
    // Switching to another audio language invalidates everything transcribed so far
    if (m_worker && currentAudioStreamIndex() != m_workerAudioStream) {
        stopWorker();
        removeTrack();
        startWorker();
    }
}

void RealtimeSubtitleController::startWorker(bool holdPlayback) {
#if OMNIPOT_HAVE_WHISPER
    stopWorker();
    if (m_mode == RealtimeSubtitleMode::Off || !m_mediaReady) return;
    if (!m_unsupportedNotice.isEmpty()) {
        m_engine->showOsdText(m_unsupportedNotice, 3000);
        return;
    }

    const QString id = modelId();
    if (!WhisperModels::isInstalled(id)) {
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: speech model missing, turn it on again from the Subtitles menu"), 4000);
        return;
    }

    RealtimeSubtitleWorker::Config config;
    config.mediaUrl = m_mediaUrl;
    config.modelPath = WhisperModels::modelPath(id);
    config.language = SettingsManager::instance().realtimeSubtitleLanguage();
    config.translate = m_mode == RealtimeSubtitleMode::TranslateToEnglish;
    config.threads = WhisperModels::recommendedThreads(cpuThreads());
    config.audioStreamIndex = currentAudioStreamIndex();
    config.duration = m_engine->duration();
    config.startPosition = m_engine->position();
    config.playbackHeld = holdPlayback;
    m_workerAudioStream = config.audioStreamIndex;

    // Signals queued by a worker that has since been replaced must be ignored
    const int generation = ++m_generation;
    m_worker = new RealtimeSubtitleWorker(config, this);
    const QString modelName = WhisperModels::info(id).displayName;
    const bool translate = config.translate;
    connect(m_worker, &RealtimeSubtitleWorker::modelReady, this, [this, generation, modelName, translate]() {
        if (generation != m_generation || m_holdingPlayback) return;
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: %1 (%2 model)")
            .arg(translate ? QStringLiteral("Translating to English") : QStringLiteral("Listening"), modelName), 2500);
    });
    connect(m_worker, &RealtimeSubtitleWorker::subtitlesUpdated, this, [this, generation](const QString &srt) {
        if (generation == m_generation) onSubtitlesUpdated(srt);
    });
    connect(m_worker, &RealtimeSubtitleWorker::failed, this, [this, generation](const QString &message) {
        if (generation != m_generation) return;
        endHold();
        m_engine->showOsdText(QStringLiteral("Realtime subtitle stopped: %1").arg(message), 4000);
    });
    connect(m_worker, &RealtimeSubtitleWorker::windowProcessed, this, [this, generation]() {
        if (generation == m_generation) endHold();
    });

    if (holdPlayback) {
        m_holdingPlayback = true;
        m_engine->pause();
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: preparing subtitles..."), kMaxHoldMs);
        QTimer::singleShot(kMaxHoldMs, this, [this, generation]() {
            if (generation == m_generation) endHold();
        });
    } else {
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: loading speech model..."), 2000);
    }
    // The worker lowers its own priority (nice 10) so playback always comes first
    m_worker->start();
#endif
}

void RealtimeSubtitleController::stopWorker() {
#if OMNIPOT_HAVE_WHISPER
    // A new file or a stop replaces the hold; the player decides on its own whether to play
    m_holdingPlayback = false;
    if (!m_worker) return;
    ++m_generation;
    m_worker->requestStop();
    m_worker->wait();
    delete m_worker;
    m_worker = nullptr;
#endif
}

void RealtimeSubtitleController::endHold() {
    if (!m_holdingPlayback) return;
    m_holdingPlayback = false;
    m_engine->showOsdText(QStringLiteral("Realtime subtitle: ready"), 1500);
    m_engine->play();
}

void RealtimeSubtitleController::removeTrack() {
    if (!m_trackAdded) return;
    m_trackAdded = false;
    mpv_handle *mpv = m_engine->rawMpvHandle();
    const int id = ownTrackId();
    if (!mpv || id < 0) return;
    const QByteArray idText = QByteArray::number(id);
    const char *cmd[] = {"sub-remove", idText.constData(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void RealtimeSubtitleController::onSubtitlesUpdated(const QString &srt) {
    mpv_handle *mpv = m_engine->rawMpvHandle();
    if (!mpv) return;

    // Replace the file atomically so mpv never reads a half-written one
    QSaveFile file(m_srtPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(srt.toUtf8()) < 0 || !file.commit()) {
        m_engine->showOsdText(QStringLiteral("Realtime subtitle: could not write %1").arg(m_srtPath), 3000);
        return;
    }

    const int id = m_trackAdded ? ownTrackId() : -1;
    if (id >= 0) {
        // sub-reload keeps the track selected if it was
        const QByteArray idText = QByteArray::number(id);
        const char *cmd[] = {"sub-reload", idText.constData(), nullptr};
        mpv_command_async(mpv, 0, cmd);
    } else {
        const QByteArray path = m_srtPath.toUtf8();
        const char *cmd[] = {"sub-add", path.constData(), "select", "Realtime subtitle", nullptr};
        mpv_command_async(mpv, 0, cmd);
        m_trackAdded = true;
    }
}

int RealtimeSubtitleController::currentAudioStreamIndex() const {
    mpv_handle *mpv = m_engine->rawMpvHandle();
    int64_t index = -1;
    if (!mpv || mpv_get_property(mpv, "current-tracks/audio/ff-index", MPV_FORMAT_INT64, &index) < 0) {
        return -1;
    }
    return static_cast<int>(index);
}

int RealtimeSubtitleController::ownTrackId() const {
    mpv_handle *mpv = m_engine->rawMpvHandle();
    if (!mpv) return -1;

    mpv_node list;
    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &list) < 0) return -1;

    int found = -1;
    if (list.format == MPV_FORMAT_NODE_ARRAY) {
        for (int i = 0; i < list.u.list->num && found < 0; ++i) {
            const mpv_node &track = list.u.list->values[i];
            if (track.format != MPV_FORMAT_NODE_MAP) continue;
            int64_t id = -1;
            bool isSub = false;
            bool isOurs = false;
            for (int k = 0; k < track.u.list->num; ++k) {
                const char *key = track.u.list->keys[k];
                const mpv_node &value = track.u.list->values[k];
                if (qstrcmp(key, "id") == 0 && value.format == MPV_FORMAT_INT64) {
                    id = value.u.int64;
                } else if (qstrcmp(key, "type") == 0 && value.format == MPV_FORMAT_STRING) {
                    isSub = qstrcmp(value.u.string, "sub") == 0;
                } else if (qstrcmp(key, "external-filename") == 0 && value.format == MPV_FORMAT_STRING) {
                    isOurs = QString::fromUtf8(value.u.string) == m_srtPath;
                }
            }
            if (isSub && isOurs) found = static_cast<int>(id);
        }
    }
    mpv_free_node_contents(&list);
    return found;
}

bool RealtimeSubtitleController::ensureModel(const QString &modelId) {
    if (WhisperModels::isInstalled(modelId)) return true;

    const WhisperModelInfo model = WhisperModels::info(modelId);
    const auto answer = QMessageBox::question(m_dialogParent, QStringLiteral("Realtime subtitle"),
        QStringLiteral("Realtime subtitle needs to download the %1 speech model (about %2 MB).\n\n"
                       "It is downloaded once and stored on this computer. After that, subtitles are made "
                       "offline: no audio ever leaves your computer.\n\nDownload it now?")
            .arg(model.displayName).arg(model.approxSizeMb),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return false;
    return downloadModel(modelId);
}

bool RealtimeSubtitleController::downloadModel(const QString &modelId) {
    const WhisperModelInfo model = WhisperModels::info(modelId);
    const QString finalPath = WhisperModels::modelPath(modelId);
    QDir().mkpath(WhisperModels::modelDirectory());

    QFile part(finalPath + QStringLiteral(".part"));
    if (!part.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(m_dialogParent, QStringLiteral("Realtime subtitle"),
            QStringLiteral("Could not save the model to %1").arg(part.fileName()));
        return false;
    }

    QNetworkAccessManager network;
    QNetworkRequest request(WhisperModels::downloadUrl(modelId));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Omnipot/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QProgressDialog progress(QStringLiteral("Downloading the %1 speech model...").arg(model.displayName),
                             QStringLiteral("Cancel"), 0, 100, m_dialogParent);
    progress.setWindowTitle(QStringLiteral("Realtime subtitle"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    QNetworkReply *reply = network.get(request);
    bool writeFailed = false;
    connect(reply, &QNetworkReply::readyRead, &progress, [&]() {
        if (part.write(reply->readAll()) < 0) {
            writeFailed = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, &progress, [&](qint64 received, qint64 total) {
        if (total <= 0) return;
        progress.setValue(static_cast<int>(received * 100 / total));
        progress.setLabelText(QStringLiteral("Downloading the %1 speech model...\n%2 MB of %3 MB")
            .arg(model.displayName).arg(received / (1024 * 1024)).arg(total / (1024 * 1024)));
    });
    connect(&progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (!writeFailed && part.write(reply->readAll()) < 0) writeFailed = true;
    part.close();

    const bool canceled = progress.wasCanceled();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkError = reply->errorString();
    const bool ok = !writeFailed && reply->error() == QNetworkReply::NoError && status == 200;
    reply->deleteLater();
    progress.close();

    if (ok) {
        QFile::remove(finalPath);
        if (part.rename(finalPath) && WhisperModels::isInstalled(modelId)) return true;
    }
    part.remove();
    QFile::remove(finalPath);

    if (!canceled) {
        QMessageBox::warning(m_dialogParent, QStringLiteral("Realtime subtitle"),
            QStringLiteral("The speech model could not be downloaded.\n\n%1")
                .arg(writeFailed ? QStringLiteral("Writing the file failed (is the disk full?)") : networkError));
    }
    return false;
}

} // namespace Omnipot
