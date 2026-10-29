#include "config/settings_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>

namespace Omnipot {

// Per-file keys must survive restarts and Qt upgrades, which qHash() does not guarantee
static QString fileKey(const QString &group, const QString &filePath) {
    const QByteArray digest = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha1);
    return group + QLatin1Char('/') + QString::fromLatin1(digest.toHex().left(16));
}

QString SettingsManager::configDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/omnipot");
}

SettingsManager& SettingsManager::instance() {
    static SettingsManager s_instance;
    return s_instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    QString configDir = configDirectory();
    QDir().mkpath(configDir);
    QString configFilePath = configDir + QStringLiteral("/config.ini");

    m_settings = std::make_unique<QSettings>(configFilePath, QSettings::IniFormat);
}

void SettingsManager::load() {
    if (m_settings) {
        m_settings->sync();
    }
}

void SettingsManager::sync() {
    if (m_settings) {
        m_settings->sync();
    }
}

QByteArray SettingsManager::windowGeometry() const {
    return m_settings->value(QStringLiteral("Window/Geometry")).toByteArray();
}

void SettingsManager::setWindowGeometry(const QByteArray &geom) {
    m_settings->setValue(QStringLiteral("Window/Geometry"), geom);
}

bool SettingsManager::isWindowMaximized() const {
    return m_settings->value(QStringLiteral("Window/Maximized"), false).toBool();
}

void SettingsManager::setWindowMaximized(bool max) {
    m_settings->setValue(QStringLiteral("Window/Maximized"), max);
}

int SettingsManager::volume() const {
    return m_settings->value(QStringLiteral("Audio/Volume"), 100).toInt();
}

void SettingsManager::setVolume(int vol) {
    m_settings->setValue(QStringLiteral("Audio/Volume"), vol);
}

bool SettingsManager::isMuted() const {
    return m_settings->value(QStringLiteral("Audio/Muted"), false).toBool();
}

void SettingsManager::setMuted(bool mute) {
    m_settings->setValue(QStringLiteral("Audio/Muted"), mute);
}

double SettingsManager::speed() const {
    return m_settings->value(QStringLiteral("Playback/Speed"), 1.0).toDouble();
}

void SettingsManager::setSpeed(double spd) {
    m_settings->setValue(QStringLiteral("Playback/Speed"), spd);
}

QString SettingsManager::defaultAspectRatio() const {
    return m_settings->value(QStringLiteral("Video/AspectRatio"), QStringLiteral("-1")).toString();
}

void SettingsManager::setDefaultAspectRatio(const QString &ratio) {
    m_settings->setValue(QStringLiteral("Video/AspectRatio"), ratio);
}

QString SettingsManager::hwdec() const {
    return m_settings->value(QStringLiteral("Video/Hwdec"), QStringLiteral("auto-safe")).toString();
}

void SettingsManager::setHwdec(const QString &mode) {
    m_settings->setValue(QStringLiteral("Video/Hwdec"), mode);
}

bool SettingsManager::rememberPlaybackPosition() const {
    return m_settings->value(QStringLiteral("Playback/RememberPosition"), true).toBool();
}

void SettingsManager::setRememberPlaybackPosition(bool enable) {
    m_settings->setValue(QStringLiteral("Playback/RememberPosition"), enable);
}

QStringList SettingsManager::recentFiles() const {
    return m_settings->value(QStringLiteral("History/RecentFiles")).toStringList();
}

void SettingsManager::addRecentFile(const QString &filePath) {
    if (filePath.isEmpty()) return;

    QStringList recents = recentFiles();
    recents.removeAll(filePath);
    recents.prepend(filePath);

    // Keep at most 30 recent files
    while (recents.size() > 30) {
        recents.removeLast();
    }

    m_settings->setValue(QStringLiteral("History/RecentFiles"), recents);
}

void SettingsManager::clearRecentFiles() {
    m_settings->remove(QStringLiteral("History/RecentFiles"));
}

double SettingsManager::resumePosition(const QString &filePath) const {
    if (filePath.isEmpty()) return 0.0;
    QString key = fileKey(QStringLiteral("Resume"), filePath);
    return m_settings->value(key, 0.0).toDouble();
}

void SettingsManager::setResumePosition(const QString &filePath, double seconds) {
    if (filePath.isEmpty()) return;
    QString key = fileKey(QStringLiteral("Resume"), filePath);
    if (seconds > 5.0) {
        m_settings->setValue(key, seconds);
    } else {
        m_settings->remove(key); // Don't remember positions within first 5 seconds
    }
}

int SettingsManager::repeatMode() const {
    return m_settings->value(QStringLiteral("Playlist/RepeatMode"), 0).toInt();
}

void SettingsManager::setRepeatMode(int mode) {
    m_settings->setValue(QStringLiteral("Playlist/RepeatMode"), mode);
}

bool SettingsManager::isShuffle() const {
    return m_settings->value(QStringLiteral("Playlist/Shuffle"), false).toBool();
}

void SettingsManager::setShuffle(bool shuf) {
    m_settings->setValue(QStringLiteral("Playlist/Shuffle"), shuf);
}

QList<SettingsManager::Bookmark> SettingsManager::bookmarks(const QString &filePath) const {
    QList<Bookmark> list;
    if (filePath.isEmpty()) return list;

    QString key = fileKey(QStringLiteral("Bookmarks"), filePath);
    QStringList rawList = m_settings->value(key).toStringList();

    for (const QString &raw : rawList) {
        int pipeIdx = raw.indexOf(QLatin1Char('|'));
        if (pipeIdx != -1) {
            Bookmark bm;
            bm.filePath = filePath;
            bm.time = raw.left(pipeIdx).toDouble();
            bm.title = raw.mid(pipeIdx + 1);
            list.append(bm);
        }
    }
    return list;
}

void SettingsManager::addBookmark(const QString &filePath, double seconds, const QString &title) {
    if (filePath.isEmpty() || seconds < 0.0) return;

    QString key = fileKey(QStringLiteral("Bookmarks"), filePath);
    QStringList rawList = m_settings->value(key).toStringList();

    QString bmTitle = title;
    if (bmTitle.isEmpty()) {
        int total = static_cast<int>(seconds);
        int h = total / 3600;
        int min = (total % 3600) / 60;
        int s = total % 60;
        bmTitle = QStringLiteral("Bookmark at %1:%2:%3").arg(h, 2, 10, QLatin1Char('0')).arg(min, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    }

    rawList.append(QStringLiteral("%1|%2").arg(seconds, 0, 'f', 2).arg(bmTitle));
    m_settings->setValue(key, rawList);
}

void SettingsManager::removeBookmark(const QString &filePath, int index) {
    if (filePath.isEmpty()) return;

    QString key = fileKey(QStringLiteral("Bookmarks"), filePath);
    QStringList rawList = m_settings->value(key).toStringList();

    if (index >= 0 && index < rawList.size()) {
        rawList.removeAt(index);
        m_settings->setValue(key, rawList);
    }
}

void SettingsManager::clearBookmarks(const QString &filePath) {
    if (filePath.isEmpty()) return;
    QString key = fileKey(QStringLiteral("Bookmarks"), filePath);
    m_settings->remove(key);
}

QString SettingsManager::videoScaler() const {
    return m_settings->value(QStringLiteral("Video/Scaler"), QStringLiteral("spline36")).toString();
}

void SettingsManager::setVideoScaler(const QString &scaler) {
    m_settings->setValue(QStringLiteral("Video/Scaler"), scaler);
}

bool SettingsManager::isAudioNormalization() const {
    return m_settings->value(QStringLiteral("Audio/Normalization"), true).toBool();
}

void SettingsManager::setAudioNormalization(bool enable) {
    m_settings->setValue(QStringLiteral("Audio/Normalization"), enable);
}

QString SettingsManager::audioChannels() const {
    return m_settings->value(QStringLiteral("Audio/Channels"), QStringLiteral("stereo")).toString();
}

void SettingsManager::setAudioChannels(const QString &channels) {
    m_settings->setValue(QStringLiteral("Audio/Channels"), channels);
}

void SettingsManager::resetToDefaults() {
    m_settings->clear();
    setVolume(100);
    setMuted(false);
    setSpeed(1.0);
    setHwdec(QStringLiteral("auto-safe"));
    setVideoScaler(QStringLiteral("spline36"));
    setAudioChannels(QStringLiteral("stereo"));
    setAudioNormalization(true);
    setRememberPlaybackPosition(true);
    setRepeatMode(0);
    setShuffle(false);
    m_settings->sync();
}

bool SettingsManager::exportSettings(const QString &filePath) const {
    if (filePath.isEmpty() || !m_settings) return false;
    m_settings->sync();
    QFile::remove(filePath);
    return QFile::copy(m_settings->fileName(), filePath);
}

bool SettingsManager::importSettings(const QString &filePath) {
    if (!QFile::exists(filePath) || !m_settings) return false;
    QString target = m_settings->fileName();
    m_settings.reset(); // Release lock
    QFile::remove(target);
    bool ok = QFile::copy(filePath, target);
    m_settings = std::make_unique<QSettings>(target, QSettings::IniFormat);
    m_settings->sync();
    return ok;
}

} // namespace Omnipot
