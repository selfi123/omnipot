#pragma once

#include <QAbstractListModel>
#include <QList>
#include "playlist/playlist_item.h"

namespace Omnipot {

class PlaylistManager : public QAbstractListModel {
    Q_OBJECT

public:
    enum PlaylistRoles {
        FilePathRole = Qt::UserRole + 1,
        DurationRole,
        TitleRole,
        IsPlayingRole
    };

    explicit PlaylistManager(QObject *parent = nullptr);
    ~PlaylistManager() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void addItem(const PlaylistItem &item);
    void addItems(const QList<PlaylistItem> &items);
    void addPath(const QString &path);
    void addFolder(const QString &folderPath, bool recursive = true);
    void removeItem(int index);
    void clear();
    void moveItem(int fromIndex, int toIndex);

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    PlaylistItem currentItem() const;
    bool hasNext() const;
    bool hasPrevious() const;

    RepeatMode repeatMode() const { return m_repeatMode; }
    void setRepeatMode(RepeatMode mode);

    bool isShuffle() const { return m_shuffle; }
    void setShuffle(bool enable);

    bool loadPlaylist(const QString &filePath);
    bool savePlaylist(const QString &filePath);

    int count() const { return m_items.size(); }
    const QList<PlaylistItem>& items() const { return m_items; }

public Q_SLOTS:
    void next();
    void previous();

Q_SIGNALS:
    void currentItemChanged(const Omnipot::PlaylistItem &item);
    void playlistChanged();
    void playbackModeChanged();

private:
    void updateShuffledIndices();
    int pickNextIndex() const;

    QList<PlaylistItem> m_items;
    int m_currentIndex = -1;
    RepeatMode m_repeatMode = RepeatMode::None;
    bool m_shuffle = false;
    QList<int> m_shuffledOrder;
};

} // namespace Omnipot
