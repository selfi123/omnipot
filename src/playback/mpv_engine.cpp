#include "playback/mpv_engine.h"
#include "config/settings_manager.h"
#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#include <clocale>
#include <cstring>

namespace Omnipot {

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

    // Sensible defaults for a desktop media player
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "terminal", "yes");
    mpv_set_option_string(m_mpv, "msg-level", "all=warn");
    mpv_set_option_string(m_mpv, "hwdec", "vaapi,auto-safe");
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    mpv_set_option_string(m_mpv, "keep-open", "always");
    mpv_set_option_string(m_mpv, "ytdl", "yes");
    mpv_set_option_string(m_mpv, "sub-auto", "fuzzy");
    mpv_set_option_string(m_mpv, "hr-seek", "yes");
    mpv_set_option_string(m_mpv, "hr-seek-framedrop", "no");
    mpv_set_option_string(m_mpv, "demuxer-max-back-bytes", "150M");
    mpv_set_option_string(m_mpv, "volume-max", "200");
    mpv_set_option_string(m_mpv, "audio-normalize-downmix", "yes");
    mpv_set_option_string(m_mpv, "audio-pitch-correction", "yes");
    mpv_set_option_string(m_mpv, "replaygain", "no");

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

    // Initialize audio normalizer per persistent configuration
    bool norm = SettingsManager::instance().isAudioNormalization();
    setNormalizerEnabled(norm);

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
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NONE);
    mpv_observe_property(m_mpv, 0, "chapter", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "chapter-list", MPV_FORMAT_NONE);
    mpv_observe_property(m_mpv, 0, "brightness", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "contrast", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "saturation", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "hue", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "video-rotate", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "deinterlace", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "audio-delay", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "sub-delay", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "audio-params/channel-count", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "video-params/gamma", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "container-fps", MPV_FORMAT_DOUBLE);

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

void MpvEngine::open(const QString &url, double startSeconds) {
    if (!m_mpv) return;

    // Applied once the file is loaded; seeking before that is rejected by mpv
    m_pendingStart = startSeconds;

    setState(PlaybackState::Opening);
    m_position = 0.0;
    m_duration = 0.0;
    Q_EMIT positionChanged(0.0);
    Q_EMIT durationChanged(0.0);

    m_metadata = MediaMetadata{};
    m_metadata.filename = QFileInfo(url).fileName();
    m_metadata.title = m_metadata.filename;
    Q_EMIT metadataChanged(m_metadata);

    // A newly opened file should play even if the previous one was paused
    int unpause = 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &unpause);

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
    const char *flags = exact ? "absolute+exact" : "absolute+keyframes";
    const char *cmd[] = {"seek", utf8Sec.constData(), flags, nullptr};
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
    pause();
    const char *cmd[] = {"frame-step", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
    showOsdText(QStringLiteral("Frame Step"));
}

void MpvEngine::frameBackStep() {
    if (!m_mpv) return;
    pause();
    const char *cmd[] = {"frame-back-step", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
    showOsdText(QStringLiteral("Frame Back Step"));
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

void MpvEngine::setAudioTrack(int id) {
    if (!m_mpv) return;
    if (id <= 0) {
        mpv_set_property_string(m_mpv, "aid", "no");
    } else {
        int64_t val = id;
        mpv_set_property_async(m_mpv, 0, "aid", MPV_FORMAT_INT64, &val);
    }
}

void MpvEngine::setSubtitleTrack(int id) {
    if (!m_mpv) return;
    if (id <= 0) {
        mpv_set_property_string(m_mpv, "sid", "no");
    } else {
        int64_t val = id;
        mpv_set_property_async(m_mpv, 0, "sid", MPV_FORMAT_INT64, &val);
    }
}

void MpvEngine::cycleAudioTrack() {
    if (!m_mpv) return;
    const char *cmd[] = {"cycle", "aid", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::cycleSubtitleTrack() {
    if (!m_mpv) return;
    const char *cmd[] = {"cycle", "sid", nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::setAudioDelay(double seconds) {
    m_audioDelay = seconds;
    if (!m_mpv) return;
    mpv_set_property_async(m_mpv, 0, "audio-delay", MPV_FORMAT_DOUBLE, &seconds);
}

void MpvEngine::setSubtitleDelay(double seconds) {
    m_subtitleDelay = seconds;
    if (!m_mpv) return;
    mpv_set_property_async(m_mpv, 0, "sub-delay", MPV_FORMAT_DOUBLE, &seconds);
}

void MpvEngine::showOsdText(const QString &text, int timeoutMs) {
    if (!m_mpv || text.isEmpty()) return;
    const QByteArray utf8Text = text.toUtf8();
    const QByteArray utf8Ms = QByteArray::number(timeoutMs);
    const char *cmd[] = {"show-text", utf8Text.constData(), utf8Ms.constData(), nullptr};
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvEngine::setNormalizerEnabled(bool enable) {
    m_normalizerEnabled = enable;
    SettingsManager::instance().setAudioNormalization(enable);
    applyAudioFilters();
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

    case MPV_EVENT_FILE_LOADED: {
        int paused = 0;
        mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused);
        setState(paused ? PlaybackState::Paused : PlaybackState::Playing);
        updateTrackList();
        updateChapterList();
        if (m_pendingStart > 0.0) {
            seek(m_pendingStart, true);
            m_pendingStart = 0.0;
        }
        break;
    }

    case MPV_EVENT_END_FILE: {
        auto *endData = static_cast<mpv_event_end_file*>(event->data);
        if (endData && endData->reason == MPV_END_FILE_REASON_ERROR) {
            setState(PlaybackState::Error);
            Q_EMIT errorOccurred(QString::fromUtf8(mpv_error_string(endData->error)));
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

    if (name == "track-list") {
        updateTrackList();
    } else if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
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
    } else if (name == "audio-params/channel-count" && prop->format == MPV_FORMAT_INT64) {
        m_metadata.audioChannels = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT metadataChanged(m_metadata);
    } else if (name == "video-params/gamma" && prop->format == MPV_FORMAT_STRING) {
        const char *gamma = *static_cast<char**>(prop->data);
        m_metadata.isHdr = gamma && (strcmp(gamma, "pq") == 0 || strcmp(gamma, "hlg") == 0);
        Q_EMIT metadataChanged(m_metadata);
    } else if (name == "container-fps" && prop->format == MPV_FORMAT_DOUBLE) {
        m_metadata.fps = *static_cast<double*>(prop->data);
        Q_EMIT metadataChanged(m_metadata);
    } else if (name == "chapter-list") {
        updateChapterList();
    } else if (name == "chapter" && prop->format == MPV_FORMAT_INT64) {
        m_currentChapter = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT chaptersChanged();
    } else if (name == "brightness" && prop->format == MPV_FORMAT_INT64) {
        m_brightness = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT colorsChanged();
    } else if (name == "contrast" && prop->format == MPV_FORMAT_INT64) {
        m_contrast = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT colorsChanged();
    } else if (name == "saturation" && prop->format == MPV_FORMAT_INT64) {
        m_saturation = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT colorsChanged();
    } else if (name == "hue" && prop->format == MPV_FORMAT_INT64) {
        m_hue = static_cast<int>(*static_cast<int64_t*>(prop->data));
        Q_EMIT colorsChanged();
    } else if (name == "video-rotate" && prop->format == MPV_FORMAT_INT64) {
        m_rotation = static_cast<int>(*static_cast<int64_t*>(prop->data));
    } else if (name == "deinterlace" && prop->format == MPV_FORMAT_FLAG) {
        m_deinterlace = (*static_cast<int*>(prop->data) != 0);
    } else if (name == "audio-delay" && prop->format == MPV_FORMAT_DOUBLE) {
        m_audioDelay = *static_cast<double*>(prop->data);
    } else if (name == "sub-delay" && prop->format == MPV_FORMAT_DOUBLE) {
        m_subtitleDelay = *static_cast<double*>(prop->data);
    }
}

void MpvEngine::updateTrackList() {
    if (!m_mpv) return;

    mpv_node node;
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
            m_audioTracks.clear();
            m_subTracks.clear();

            for (int i = 0; i < node.u.list->num; ++i) {
                mpv_node *trackNode = &node.u.list->values[i];
                if (trackNode->format != MPV_FORMAT_NODE_MAP || !trackNode->u.list) continue;

                TrackInfo track;
                for (int j = 0; j < trackNode->u.list->num; ++j) {
                    const char *key = trackNode->u.list->keys[j];
                    mpv_node *val = &trackNode->u.list->values[j];
                    if (!key || !val) continue;

                    if (strcmp(key, "id") == 0 && val->format == MPV_FORMAT_INT64) {
                        track.id = static_cast<int>(val->u.int64);
                    } else if (strcmp(key, "type") == 0 && val->format == MPV_FORMAT_STRING && val->u.string) {
                        track.type = QString::fromUtf8(val->u.string);
                    } else if (strcmp(key, "title") == 0 && val->format == MPV_FORMAT_STRING && val->u.string) {
                        track.title = QString::fromUtf8(val->u.string);
                    } else if (strcmp(key, "lang") == 0 && val->format == MPV_FORMAT_STRING && val->u.string) {
                        track.language = QString::fromUtf8(val->u.string).toUpper();
                    } else if (strcmp(key, "codec") == 0 && val->format == MPV_FORMAT_STRING && val->u.string) {
                        track.codec = QString::fromUtf8(val->u.string).toUpper();
                    } else if (strcmp(key, "selected") == 0 && val->format == MPV_FORMAT_FLAG) {
                        track.isSelected = (val->u.flag != 0);
                    } else if (strcmp(key, "default") == 0 && val->format == MPV_FORMAT_FLAG) {
                        track.isDefault = (val->u.flag != 0);
                    }
                }

                if (track.type == QStringLiteral("audio")) {
                    m_audioTracks.append(track);
                } else if (track.type == QStringLiteral("sub")) {
                    m_subTracks.append(track);
                }
            }
            Q_EMIT tracksChanged();
        }
        mpv_free_node_contents(&node);
    }
}

void MpvEngine::updateChapterList() {
    if (!m_mpv) return;

    mpv_node node;
    if (mpv_get_property(m_mpv, "chapter-list", MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
            m_chapters.clear();
            for (int i = 0; i < node.u.list->num; ++i) {
                mpv_node *chNode = &node.u.list->values[i];
                if (chNode->format != MPV_FORMAT_NODE_MAP || !chNode->u.list) continue;

                ChapterInfo info;
                info.id = i;
                for (int j = 0; j < chNode->u.list->num; ++j) {
                    const char *key = chNode->u.list->keys[j];
                    mpv_node *val = &chNode->u.list->values[j];
                    if (!key || !val) continue;

                    if (strcmp(key, "title") == 0 && val->format == MPV_FORMAT_STRING && val->u.string) {
                        info.title = QString::fromUtf8(val->u.string);
                    } else if (strcmp(key, "time") == 0 && val->format == MPV_FORMAT_DOUBLE) {
                        info.time = val->u.double_;
                    }
                }
                if (info.title.isEmpty()) {
                    info.title = QStringLiteral("Chapter %1").arg(i + 1);
                }
                m_chapters.append(info);
            }
            Q_EMIT chaptersChanged();
        }
        mpv_free_node_contents(&node);
    }
}

double MpvEngine::equalizerBand(int bandIndex) const {
    if (bandIndex >= 0 && bandIndex < 10) {
        return m_eqGains[bandIndex];
    }
    return 0.0;
}

void MpvEngine::setHwdec(const QString &mode) {
    m_hwdec = mode;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "hwdec", mode.toUtf8().constData());
    }
}

void MpvEngine::setBrightness(int value) {
    m_brightness = std::clamp(value, -100, 100);
    if (m_mpv) {
        int64_t v = m_brightness;
        mpv_set_property(m_mpv, "brightness", MPV_FORMAT_INT64, &v);
    }
    Q_EMIT colorsChanged();
}

void MpvEngine::setContrast(int value) {
    m_contrast = std::clamp(value, -100, 100);
    if (m_mpv) {
        int64_t v = m_contrast;
        mpv_set_property(m_mpv, "contrast", MPV_FORMAT_INT64, &v);
    }
    Q_EMIT colorsChanged();
}

void MpvEngine::setSaturation(int value) {
    m_saturation = std::clamp(value, -100, 100);
    if (m_mpv) {
        int64_t v = m_saturation;
        mpv_set_property(m_mpv, "saturation", MPV_FORMAT_INT64, &v);
    }
    Q_EMIT colorsChanged();
}

void MpvEngine::setHue(int value) {
    m_hue = std::clamp(value, -100, 100);
    if (m_mpv) {
        int64_t v = m_hue;
        mpv_set_property(m_mpv, "hue", MPV_FORMAT_INT64, &v);
    }
    Q_EMIT colorsChanged();
}

void MpvEngine::resetColors() {
    setBrightness(0);
    setContrast(0);
    setSaturation(0);
    setHue(0);
}

void MpvEngine::setEqualizerEnabled(bool enabled) {
    m_equalizerEnabled = enabled;
    applyEqualizer();
    Q_EMIT equalizerChanged();
}

void MpvEngine::setEqualizerBand(int bandIndex, double gainDb) {
    if (bandIndex >= 0 && bandIndex < 10) {
        m_eqGains[bandIndex] = std::clamp(gainDb, -12.0, 12.0);
        if (m_equalizerEnabled) {
            applyEqualizer();
        }
        Q_EMIT equalizerChanged();
    }
}

void MpvEngine::setEqualizerPreset(const QString &presetName) {
    static const QMap<QString, std::array<double, 10>> presets = {
        {QStringLiteral("Flat"), {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {QStringLiteral("Rock"), {4.5, 3.5, 2.0, 0.0, -1.5, -1.0, 1.0, 2.5, 3.5, 4.0}},
        {QStringLiteral("Pop"), {-1.5, 1.0, 3.0, 3.5, 2.0, -1.0, -1.5, -1.5, 1.0, 2.5}},
        {QStringLiteral("Classical"), {4.5, 3.5, 3.0, 2.5, -1.5, -1.5, 0.0, 2.0, 3.0, 3.5}},
        {QStringLiteral("Jazz"), {3.5, 2.5, 1.0, 2.0, -1.5, -1.5, 0.0, 1.5, 3.0, 3.5}},
        {QStringLiteral("Club"), {0.0, 0.0, 2.0, 3.5, 3.5, 3.5, 2.0, 0.0, 0.0, 0.0}},
        {QStringLiteral("Vocal"), {-2.0, -3.0, -1.5, 2.0, 4.0, 4.0, 2.5, 1.0, 0.0, -2.0}}
    };

    auto it = presets.find(presetName);
    if (it != presets.end()) {
        m_eqGains = it.value();
        m_equalizerEnabled = true;
        applyEqualizer();
        Q_EMIT equalizerChanged();
    }
}

void MpvEngine::resetEqualizer() {
    m_eqGains.fill(0.0);
    applyEqualizer();
    Q_EMIT equalizerChanged();
}

void MpvEngine::applyEqualizer() {
    applyAudioFilters();
}

void MpvEngine::applyAudioFilters() {
    if (!m_mpv) return;

    QStringList filters;

    if (m_normalizerEnabled) {
        // High-fidelity dynamic range normalizer
        // Boosts quiet dialogue up to +20dB while preventing clipping on peaks
        filters << QStringLiteral("dynaudnorm=f=75:g=25:p=0.95:m=10.0:r=0.9");
    }

    if (m_equalizerEnabled) {
        static const int freqs[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        for (int i = 0; i < 10; ++i) {
            filters << QStringLiteral("equalizer=f=%1:width_type=o:w=1:g=%2").arg(freqs[i]).arg(m_eqGains[i], 0, 'f', 1);
        }
    }

    if (filters.isEmpty()) {
        mpv_set_property_string(m_mpv, "af", "");
    } else {
        QString afString = QStringLiteral("lavfi=[%1]").arg(filters.join(QLatin1Char(',')));
        mpv_set_property_string(m_mpv, "af", afString.toUtf8().constData());
    }
}

void MpvEngine::setRotation(int degrees) {
    m_rotation = degrees % 360;
    if (m_rotation < 0) m_rotation += 360;
    if (m_mpv) {
        int64_t rot = m_rotation;
        mpv_set_property(m_mpv, "video-rotate", MPV_FORMAT_INT64, &rot);
    }
}

void MpvEngine::rotateClockwise() {
    setRotation((m_rotation + 90) % 360);
}

void MpvEngine::setHorizontalFlip(bool flip) {
    m_hflip = flip;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "video-hflip", flip ? "yes" : "no");
    }
}

void MpvEngine::setVerticalFlip(bool flip) {
    m_vflip = flip;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "video-vflip", flip ? "yes" : "no");
    }
}

void MpvEngine::setDeinterlacing(bool enable) {
    m_deinterlace = enable;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "deinterlace", enable ? "yes" : "no");
    }
}

void MpvEngine::setSubtitleFontSize(int size) {
    m_subtitleFontSize = std::clamp(size, 10, 120);
    if (m_mpv) {
        int64_t sz = m_subtitleFontSize;
        mpv_set_property(m_mpv, "sub-font-size", MPV_FORMAT_INT64, &sz);
    }
}

void MpvEngine::setSubtitlePosition(int pos) {
    m_subtitlePosition = std::clamp(pos, 0, 100);
    if (m_mpv) {
        int64_t p = m_subtitlePosition;
        mpv_set_property(m_mpv, "sub-pos", MPV_FORMAT_INT64, &p);
    }
}

void MpvEngine::setAssOverride(bool override) {
    m_assOverride = override;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "sub-ass-override", override ? "force" : "no");
    }
}

void MpvEngine::setChapter(int index) {
    if (!m_mpv || index < 0 || index >= m_chapters.size()) return;
    int64_t ch = index;
    mpv_set_property(m_mpv, "chapter", MPV_FORMAT_INT64, &ch);
}

void MpvEngine::nextChapter() {
    if (!m_mpv || m_chapters.isEmpty()) return;
    int next = m_currentChapter + 1;
    if (next < m_chapters.size()) {
        setChapter(next);
    }
}

void MpvEngine::previousChapter() {
    if (!m_mpv || m_chapters.isEmpty()) return;
    int prev = m_currentChapter - 1;
    if (prev >= 0) {
        setChapter(prev);
    }
}

void MpvEngine::setLoopA(double seconds) {
    m_loopA = seconds;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "ab-loop-a", QString::number(seconds, 'f', 3).toUtf8().constData());
    }
}

void MpvEngine::setLoopB(double seconds) {
    m_loopB = seconds;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "ab-loop-b", QString::number(seconds, 'f', 3).toUtf8().constData());
    }
}

void MpvEngine::clearLoop() {
    m_loopA = -1.0;
    m_loopB = -1.0;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "ab-loop-a", "no");
        mpv_set_property_string(m_mpv, "ab-loop-b", "no");
    }
}

StreamStats MpvEngine::streamStats() const {
    StreamStats stats;
    if (!m_mpv) return stats;

    stats.videoCodec = m_metadata.videoCodec;
    stats.audioCodec = m_metadata.audioCodec;
    stats.videoWidth = m_metadata.videoWidth;
    stats.videoHeight = m_metadata.videoHeight;

    char *hw = mpv_get_property_string(m_mpv, "hwdec-current");
    if (hw) {
        stats.hwdec = QString::fromUtf8(hw);
        mpv_free(hw);
    }

    double fps = 0.0;
    if (mpv_get_property(m_mpv, "estimated-vf-fps", MPV_FORMAT_DOUBLE, &fps) >= 0 && fps > 0.0) {
        stats.fps = fps;
    } else {
        stats.fps = m_metadata.fps;
    }

    int64_t vBitrate = 0;
    if (mpv_get_property(m_mpv, "video-bitrate", MPV_FORMAT_INT64, &vBitrate) >= 0) {
        stats.videoBitrate = vBitrate;
    }

    int64_t aBitrate = 0;
    if (mpv_get_property(m_mpv, "audio-bitrate", MPV_FORMAT_INT64, &aBitrate) >= 0) {
        stats.audioBitrate = aBitrate;
    }

    int64_t sRate = 0;
    if (mpv_get_property(m_mpv, "audio-params/samplerate", MPV_FORMAT_INT64, &sRate) >= 0) {
        stats.audioSampleRate = static_cast<int>(sRate);
    }

    int64_t chans = 0;
    if (mpv_get_property(m_mpv, "audio-params/channel-count", MPV_FORMAT_INT64, &chans) >= 0) {
        stats.audioChannels = static_cast<int>(chans);
    }

    int64_t dropped = 0;
    if (mpv_get_property(m_mpv, "drop-frame-count", MPV_FORMAT_INT64, &dropped) >= 0) {
        stats.droppedFrames = dropped;
    }

    double avsync = 0.0;
    if (mpv_get_property(m_mpv, "avsync", MPV_FORMAT_DOUBLE, &avsync) >= 0) {
        stats.avSyncDiff = avsync;
    }

    return stats;
}

void MpvEngine::setState(PlaybackState state) {
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged(m_state);
    }
}

QStringList MpvEngine::audioDevices() const {
    QStringList devices;
    if (!m_mpv) return devices;

    devices.append(QStringLiteral("auto|System Default (Auto)"));

    mpv_node node;
    if (mpv_get_property(m_mpv, "audio-device-list", MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_ARRAY) {
            for (int i = 0; i < node.u.list->num; ++i) {
                mpv_node *item = &node.u.list->values[i];
                if (item->format == MPV_FORMAT_NODE_MAP) {
                    QString name;
                    QString desc;
                    for (int j = 0; j < item->u.list->num; ++j) {
                        const char *k = item->u.list->keys[j];
                        if (strcmp(k, "name") == 0 && item->u.list->values[j].format == MPV_FORMAT_STRING) {
                            name = QString::fromUtf8(item->u.list->values[j].u.string);
                        } else if (strcmp(k, "description") == 0 && item->u.list->values[j].format == MPV_FORMAT_STRING) {
                            desc = QString::fromUtf8(item->u.list->values[j].u.string);
                        }
                    }
                    if (!name.isEmpty() && name != QStringLiteral("auto")) {
                        devices.append(QStringLiteral("%1|%2").arg(name, desc.isEmpty() ? name : desc));
                    }
                }
            }
        }
        mpv_free_node_contents(&node);
    }
    return devices;
}

void MpvEngine::setAudioDevice(const QString &device) {
    m_audioDevice = device;
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "audio-device", device.toUtf8().constData());
    }
}

} // namespace Omnipot
