#include "playlist/m3u_parser.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

namespace PotPlayer {

QList<PlaylistItem> M3uParser::load(const QString &filePath) {
    QList<PlaylistItem> items;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return items;
    }

    QDir baseDir = QFileInfo(filePath).dir();
    QTextStream in(&file);
    QString pendingTitle;
    double pendingDuration = 0.0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QStringLiteral("#EXTINF:"))) {
            // #EXTINF:123,Artist - Title
            int commaIdx = line.indexOf(QLatin1Char(','));
            if (commaIdx != -1) {
                QString durStr = line.mid(8, commaIdx - 8).trimmed();
                pendingDuration = durStr.toDouble();
                if (pendingDuration < 0.0) pendingDuration = 0.0;
                pendingTitle = line.mid(commaIdx + 1).trimmed();
            }
        } else if (!line.startsWith(QLatin1Char('#'))) {
            PlaylistItem item;
            if (line.contains(QStringLiteral("://")) || QDir::isAbsolutePath(line)) {
                item.filePath = line;
            } else {
                item.filePath = baseDir.absoluteFilePath(line);
            }

            item.title = pendingTitle.isEmpty() ? QFileInfo(item.filePath).fileName() : pendingTitle;
            item.duration = pendingDuration;
            items.append(item);

            pendingTitle.clear();
            pendingDuration = 0.0;
        }
    }

    return items;
}

bool M3uParser::save(const QString &filePath, const QList<PlaylistItem> &items) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out << "#EXTM3U\n";

    for (const auto &item : items) {
        qint64 sec = static_cast<qint64>(item.duration);
        out << "#EXTINF:" << sec << "," << item.title << "\n";
        out << item.filePath << "\n";
    }

    return true;
}

} // namespace PotPlayer
