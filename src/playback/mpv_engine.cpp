#include "playback/mpv_engine.h"
#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#include <clocale>

namespace PotPlayer {

void MpvEngine::onMpvWakeup(void *ctx) {
    auto *self = static_cast<MpvEngine*>(ctx);
    if (self && self->m_eventTimer) {
        QMetaObject::invokeMethod(self->m_eventTimer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
    }
}

MpvEngine::MpvEngine(QObject *parent)
    : PlaybackEngine(parent)
{
}

MpvEngine::~MpvEngine() {
    shutdown();
}

bool MpvEngine::initialize() {
    if (m_mpv) {
        return true;
    }

    // Qt sets the LC_NUMERIC to system locale; mpv requires C numeric locale
    std::setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv) {
        qCritical() << "[MpvEngine] Failed to create mpv instance";
        return false;
    }

    // Sensible defaults matching modern PotPlayer behavior
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "terminal", "yes");
    mpv_set_option_string(m_mpv, "msg-level", "all=v");
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "vo", "gpu");
    mpv_set_option_string(m_mpv, "keep-open", "always");
    mpv_set_option_string(m_mpv, "ytdl", "yes");
    mpv_set_option_string(m_mpv, "sub-auto", "fuzzy");

    int status = mpv_initialize(m_mpv);
    if (status < 0) {
        qCritical() << "[MpvEngine] Failed to initialize mpv:" << mpv_error_string(status);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    // Set wakeup callback
    mpv_set_wakeup_callback(m_mpv, onMpvWakeup, this);

    m_eventTimer = new QTimer(this);
    m_eventTimer->setSingleShot(true);
    connect(m_eventTimer, &QTimer::timeout, this, &MpvEngine::processMpvEvents);

    // Observe essential playback properties
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "video-format", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "audio-codec-name", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "hwdec-current", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "width", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "height", MPV_FORMAT_INT64);

    setState(PlaybackState::Idle);
    return true;
}

void MpvEngine::shutdown() {
    if (m_eventTimer) {
        m_eventTimer->stop();
    }
    if (m_mpv) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
    setState(PlaybackState::Idle);
}

void MpvEngine::setWid(int64_t wid) {
    if (m_mpv) {
        mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid);
    }
}

void MpvEngine::open(const QString &url) {
    if (!m_mpv) return;

    setState(PlaybackState::Opening);
    m_position = 0.0;
    m_duration = 0.0;
    Q_EMIT positionChanged(0.0);
    Q_EMIT durationChanged(0.0);

    m_metadata = MediaMetadata{};
    m_metadata.filename = QFileInfo(url).fileName();
    m_metadata.title = m_metadata.filename;
    Q_EMIT metadataChanged(m_metadata);

    const QByteArray utf8Url = url.toUtf8();
    const char *cmd[] = {"loadfile", utf8Url.constData(), nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::play() {
    if (!m_mpv) return;
    int flag = 0;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvEngine::pause() {
    if (!m_mpv) return;
    int flag = 1;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvEngine::stop() {
    if (!m_mpv) return;
    const char *cmd[] = {"stop", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
    setState(PlaybackState::Stopped);
    m_position = 0.0;
    Q_EMIT positionChanged(0.0);
}

void MpvEngine::togglePlayPause() {
    if (!m_mpv) return;
    const char *cmd[] = {"cycle", "pause", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::seek(double seconds, bool exact) {
    if (!m_mpv) return;
    const QString secStr = QString::number(std::max(0.0, seconds), 'f', 3);
    const QByteArray utf8Sec = secStr.toUtf8();
    const char *mode = exact ? "exact" : "relative";
    const char *cmd[] = {"seek", utf8Sec.constData(), "absolute", mode, nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::seekRelative(double offsetSeconds) {
    if (!m_mpv) return;
    const QString secStr = QString::number(offsetSeconds, 'f', 3);
    const QByteArray utf8Sec = secStr.toUtf8();
    const char *cmd[] = {"seek", utf8Sec.constData(), "relative", "exact", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::frameStep() {
    if (!m_mpv) return;
    const char *cmd[] = {"frame-step", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::frameBackStep() {
    if (!m_mpv) return;
    const char *cmd[] = {"frame-back-step", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::setVolume(int volume) {
    if (!m_mpv) return;
    int clamped = std::clamp(volume, 0, 200);
    int64_t val = clamped;
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_INT64, &val);
}

void MpvEngine::setMuted(bool muted) {
    if (!m_mpv) return;
    int flag = muted ? 1 : 0;
    mpv_set_property_async(m_mpv, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvEngine::toggleMute() {
    if (!m_mpv) return;
    const char *cmd[] = {"cycle", "mute", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::setSpeed(double speed) {
    if (!m_mpv) return;
    double clamped = std::clamp(speed, 0.2, 12.0);
    mpv_set_property_async(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE, &clamped);
}

void MpvEngine::setAspectRatio(const QString &aspectRatio) {
    if (!m_mpv) return;
    const QByteArray utf8Aspect = aspectRatio.toUtf8();
    mpv_set_property_string(m_mpv, "video-aspect-override", utf8Aspect.constData());
}

void MpvEngine::processMpvEvents() {
    if (!m_mpv) return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handleMpvEvent(event);
    }
}

void MpvEngine::handleMpvEvent(mpv_event *event) {
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE:
        handlePropertyChange(static_cast<mpv_event_property*>(event->data));
        break;

    case MPV_EVENT_START_FILE:
        setState(PlaybackState::Opening);
        break;

    case MPV_EVENT_FILE_LOADED:
        setState(PlaybackState::Playing);
        break;

    case MPV_EVENT_END_FILE: {
        auto *endData = static_cast<mpv_event_end_file*>(event->data);
        if (endData && endData->reason == MPV_END_FILE_REASON_ERROR) {
            setState(PlaybackState::Error);
            Q_EMIT errorOccurred(QString::fromUtf8(mpv_error_string(endData->error_code)));
        } else if (endData && endData->reason == MPV_END_FILE_REASON_EOF) {
            Q_EMIT eofReached();
        }
        break;
    }

    default:
        break;
    }
}

void MpvEngine::handlePropertyChange(mpv_event_property *prop) {
    if (!prop || !prop->name) return;

    const std::string name(prop->name);

    if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
        m_position = *static_cast<double*>(prop->data);
        Q_EMIT positionChanged(m_position);
    } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
        m_duration = *static_cast<double*>(prop->data);
        m_metadata.duration = m_duration;
        Q_EMIT durationChanged(m_duration);
        Q_EMIT metadataChanged(m_metadata);
    } else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
        int paused = *static_cast<int*>(prop->data);
        if (m_state == PlaybackState::Playing && paused) {
            setState(PlaybackState::Paused);
        } else if (m_state == PlaybackState::Paused && !paused) {
            setState(PlaybackState::Playing);
        }
    } else if (name == "volume" && prop->format == MPV_FORMAT_INT64) {
        m_volume = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT volumeChanged(m_volume);
    } else if (name == "mute" && prop->format == MPV_FORMAT_FLAG) {
        m_isMuted = (*static_cast<int*>(prop->data) != 0);
        Q_EMIT muteChanged(m_isMuted);
    } else if (name == "speed" && prop->format == MPV_FORMAT_DOUBLE) {
        m_speed = *static_cast<double*>(prop->data);
        Q_EMIT speedChanged(m_speed);
    } else if (name == "media-title" && prop->format == MPV_FORMAT_STRING) {
        const char *title = *static_cast<char**>(prop->data);
        if (title) {
            m_metadata.title = QString::fromUtf8(title);
            Q_EMIT metadataChanged(m_metadata);
        }
    } else if (name == "video-format" && prop->format == MPV_FORMAT_STRING) {
        const char *vfmt = *static_cast<char**>(prop->data);
        if (vfmt) {
            m_metadata.videoCodec = QString::fromUtf8(vfmt).toUpper();
            Q_EMIT metadataChanged(m_metadata);
        }
    } else if (name == "audio-codec-name" && prop->format == MPV_FORMAT_STRING) {
        const char *afmt = *static_cast<char**>(prop->data);
        if (afmt) {
            m_metadata.audioCodec = QString::fromUtf8(afmt).toUpper();
            Q_EMIT metadataChanged(m_metadata);
        }
    } else if (name == "hwdec-current" && prop->format == MPV_FORMAT_STRING) {
        const char *hw = *static_cast<char**>(prop->data);
        m_metadata.isHardwareDecoded = (hw != nullptr && strcmp(hw, "no") != 0 && strlen(hw) > 0);
        Q_EMIT metadataChanged(m_metadata);
    } else if (name == "width" && prop->format == MPV_FORMAT_INT64) {
        m_metadata.videoWidth = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT metadataChanged(m_metadata);
    } else if (name == "height" && prop->format == MPV_FORMAT_INT64) {
        m_metadata.videoHeight = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT metadataChanged(m_metadata);
    }
}

void MpvEngine::setState(PlaybackState state) {
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged(m_state);
    }
}

} // namespace PotPlayer
