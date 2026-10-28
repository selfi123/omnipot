#pragma once

#include "playlist/playlist_item.h"
#include <QList>
#include <QString>

namespace PotPlayer {

class M3uParser {
public:
    static QList<PlaylistItem> load(const QString &filePath);
    static bool save(const QString &filePath, const QList<PlaylistItem> &items);
};

} // namespace PotPlayer
