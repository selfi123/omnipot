#pragma once

#include "playlist/playlist_item.h"
#include <QList>
#include <QString>

namespace Omnipot {

class DplParser {
public:
    static QList<PlaylistItem> load(const QString &filePath, QString *outCurrentPlaying = nullptr, double *outPlaytime = nullptr);
    static bool save(const QString &filePath, const QList<PlaylistItem> &items, const QString &currentPlaying = QString(), double playtime = 0.0);
};

} // namespace Omnipot
