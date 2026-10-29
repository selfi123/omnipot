#include "playlist/playlist_manager.h"
#include "playlist/dpl_parser.h"
#include "playlist/m3u_parser.h"
#include <QFileInfo>
#include <QDirIterator>
#include <QRandomGenerator>
#include <algorithm>

namespace Omnipot {

static const QStringList s_supportedMediaExtensions = {
    QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
    QStringLiteral("webm"), QStringLiteral("flv"), QStringLiteral("ts"),
    QStringLiteral("m4v"), QStringLiteral("mov"), QStringLiteral("wmv"),
    QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("aac"),
    QStringLiteral("ogg"), QStringLiteral("opus"), QStringLiteral("wav")
};

PlaylistManager::PlaylistManager(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlaylistManager::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant PlaylistManager::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const auto &item = m_items.at(index.row());

    switch (role) {
    case Qt::DisplayRole: {
        QString prefix = (index.row() == m_currentIndex) ? QStringLiteral("▶ ") : QStringLiteral("   ");
        return QStringLiteral("%1%2. %3").arg(prefix).arg(index.row() + 1).arg(item.title);
    }
    case FilePathRole:
        return item.filePath;
    case TitleRole:
        return item.title;
    case DurationRole:
        return item.duration;
    case IsPlayingRole:
        return (index.row() == m_currentIndex);
    default:
        return QVariant();
    }
}

void PlaylistManager::addItem(const PlaylistItem &item) {
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(item);
    endInsertRows();

    updateShuffledIndices();
    Q_EMIT playlistChanged();
}

void PlaylistManager::addItems(const QList<PlaylistItem> &items) {
    if (items.isEmpty()) return;
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size() + items.size() - 1);
    m_items.append(items);
    endInsertRows();

    updateShuffledIndices();
    Q_EMIT playlistChanged();
}

void PlaylistManager::addPath(const QString &path) {
    QFileInfo fi(path);
    if (fi.isDir()) {
        addFolder(path, true);
    } else if (fi.exists() || path.contains(QStringLiteral("://"))) {
        PlaylistItem item;
        item.filePath = path;
        item.title = fi.fileName();
        addItem(item);
    }
}

void PlaylistManager::addFolder(const QString &folderPath, bool recursive) {
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(folderPath, flags);

    QList<PlaylistItem> newItems;
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (fi.isFile() && s_supportedMediaExtensions.contains(fi.suffix().toLower())) {
            PlaylistItem item;
            item.filePath = fi.absoluteFilePath();
            item.title = fi.fileName();
            newItems.append(item);
        }
    }

    if (!newItems.isEmpty()) {
        addItems(newItems);
    }
}

void PlaylistManager::removeItem(int index) {
    if (index < 0 || index >= m_items.size()) return;

    beginRemoveRows(QModelIndex(), index, index);
    m_items.removeAt(index);
    endRemoveRows();

    if (m_currentIndex == index) {
        if (m_currentIndex >= m_items.size()) {
            m_currentIndex = m_items.size() - 1;
        }
        if (m_currentIndex >= 0) {
            Q_EMIT currentItemChanged(m_items.at(m_currentIndex));
        }
    } else if (m_currentIndex > index) {
        m_currentIndex--;
    }

    updateShuffledIndices();
    Q_EMIT playlistChanged();
}

void PlaylistManager::clear() {
    beginResetModel();
    m_items.clear();
    m_currentIndex = -1;
    m_shuffledOrder.clear();
    endResetModel();

    Q_EMIT playlistChanged();
}

void PlaylistManager::moveItem(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= m_items.size() ||
        toIndex < 0 || toIndex >= m_items.size() || fromIndex == toIndex) {
        return;
    }

    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), toIndex > fromIndex ? toIndex + 1 : toIndex);
    m_items.move(fromIndex, toIndex);
    endMoveRows();

    if (m_currentIndex == fromIndex) {
        m_currentIndex = toIndex;
    } else if (fromIndex < m_currentIndex && toIndex >= m_currentIndex) {
        m_currentIndex--;
    } else if (fromIndex > m_currentIndex && toIndex <= m_currentIndex) {
        m_currentIndex++;
    }

    updateShuffledIndices();
    Q_EMIT playlistChanged();
}

void PlaylistManager::setCurrentIndex(int index) {
    if (index < 0 || index >= m_items.size() || index == m_currentIndex) {
        return;
    }

    int oldIndex = m_currentIndex;
    m_currentIndex = index;

    if (oldIndex >= 0 && oldIndex < m_items.size()) {
        Q_EMIT dataChanged(createIndex(oldIndex, 0), createIndex(oldIndex, 0), {Qt::DisplayRole, IsPlayingRole});
    }
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size()) {
        Q_EMIT dataChanged(createIndex(m_currentIndex, 0), createIndex(m_currentIndex, 0), {Qt::DisplayRole, IsPlayingRole});
        Q_EMIT currentItemChanged(m_items.at(m_currentIndex));
    }
}

PlaylistItem PlaylistManager::currentItem() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size()) {
        return m_items.at(m_currentIndex);
    }
    return PlaylistItem{};
}

bool PlaylistManager::hasNext() const {
    if (m_items.isEmpty()) return false;
    if (m_repeatMode == RepeatMode::All || m_repeatMode == RepeatMode::Track) return true;
    return m_currentIndex < m_items.size() - 1;
}

bool PlaylistManager::hasPrevious() const {
    if (m_items.isEmpty()) return false;
    if (m_repeatMode == RepeatMode::All) return true;
    return m_currentIndex > 0;
}

void PlaylistManager::next() {
    if (m_items.isEmpty()) return;

    if (m_repeatMode == RepeatMode::Track) {
        if (m_currentIndex >= 0) {
            Q_EMIT currentItemChanged(m_items.at(m_currentIndex));
            return;
        }
    }

    int nextIdx = pickNextIndex();
    if (nextIdx != -1) {
        setCurrentIndex(nextIdx);
    }
}

void PlaylistManager::previous() {
    if (m_items.isEmpty()) return;

    int prevIdx = m_currentIndex - 1;
    if (prevIdx < 0) {
        if (m_repeatMode == RepeatMode::All) {
            prevIdx = m_items.size() - 1;
        } else {
            return;
        }
    }
    setCurrentIndex(prevIdx);
}

void PlaylistManager::setRepeatMode(RepeatMode mode) {
    if (m_repeatMode == mode) return;
    m_repeatMode = mode;
    Q_EMIT playbackModeChanged();
}

void PlaylistManager::setShuffle(bool enable) {
    if (m_shuffle == enable) return;
    m_shuffle = enable;
    updateShuffledIndices();
    Q_EMIT playbackModeChanged();
}

void PlaylistManager::updateShuffledIndices() {
    m_shuffledOrder.clear();
    for (int i = 0; i < m_items.size(); ++i) {
        m_shuffledOrder.append(i);
    }
    if (m_shuffle) {
        auto *rng = QRandomGenerator::global();
        for (int i = m_shuffledOrder.size() - 1; i > 0; --i) {
            int j = rng->bounded(i + 1);
            m_shuffledOrder.swapItemsAt(i, j);
        }
    }
}

int PlaylistManager::pickNextIndex() const {
    if (m_items.isEmpty()) return -1;

    if (m_shuffle) {
        int posInShuffled = m_shuffledOrder.indexOf(m_currentIndex);
        if (posInShuffled != -1 && posInShuffled < m_shuffledOrder.size() - 1) {
            return m_shuffledOrder.at(posInShuffled + 1);
        } else if (m_repeatMode == RepeatMode::All && !m_shuffledOrder.isEmpty()) {
            return m_shuffledOrder.first();
        }
        return -1;
    }

    int nextIdx = m_currentIndex + 1;
    if (nextIdx >= m_items.size()) {
        if (m_repeatMode == RepeatMode::All) {
            return 0;
        }
        return -1;
    }
    return nextIdx;
}

bool PlaylistManager::loadPlaylist(const QString &filePath) {
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    QList<PlaylistItem> loaded;
    if (ext == QStringLiteral("dpl")) {
        loaded = DplParser::load(filePath);
    } else if (ext == QStringLiteral("m3u") || ext == QStringLiteral("m3u8")) {
        loaded = M3uParser::load(filePath);
    }

    if (!loaded.isEmpty()) {
        clear();
        addItems(loaded);
        setCurrentIndex(0);
        return true;
    }
    return false;
}

bool PlaylistManager::savePlaylist(const QString &filePath) {
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext == QStringLiteral("dpl")) {
        QString currentFile = (m_currentIndex >= 0 && m_currentIndex < m_items.size()) ? m_items.at(m_currentIndex).filePath : QString();
        return DplParser::save(filePath, m_items, currentFile, 0.0);
    } else if (ext == QStringLiteral("m3u") || ext == QStringLiteral("m3u8")) {
        return M3uParser::save(filePath, m_items);
    }
    return false;
}

} // namespace Omnipot
