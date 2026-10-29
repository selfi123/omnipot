#include "playlist/dpl_parser.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>
#include <QMap>

namespace Omnipot {

QList<PlaylistItem> DplParser::load(const QString &filePath, QString *outCurrentPlaying, double *outPlaytime) {
    QList<PlaylistItem> items;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return items;
    }

    QTextStream in(&file);
    QString header = in.readLine().trimmed();
    if (header != QStringLiteral("DAUMPLAYLIST")) {
        return items;
    }

    struct RawItem {
        QString file;
        QString title;
        double duration = 0.0;
        double start = 0.0;
    };
    QMap<int, RawItem> rawMap;

    static const QRegularExpression propRegex(QStringLiteral("^(\\d+)\\*([a-zA-Z0-9_]+)\\*(.*)$"));

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QStringLiteral("playname="))) {
            if (outCurrentPlaying) {
                *outCurrentPlaying = line.mid(9);
            }
        } else if (line.startsWith(QStringLiteral("playtime="))) {
            if (outPlaytime) {
                bool ok = false;
                double ms = line.mid(9).toDouble(&ok);
                if (ok) *outPlaytime = ms / 1000.0;
            }
        } else {
            auto match = propRegex.match(line);
            if (match.hasMatch()) {
                int index = match.captured(1).toInt();
                QString prop = match.captured(2).toLower();
                QString val = match.captured(3);

                if (prop == QStringLiteral("file")) {
                    rawMap[index].file = val;
                } else if (prop == QStringLiteral("title")) {
                    rawMap[index].title = val;
                } else if (prop == QStringLiteral("duration2") || prop == QStringLiteral("duration")) {
                    bool ok = false;
                    double ms = val.toDouble(&ok);
                    if (ok) rawMap[index].duration = ms / 1000.0;
                } else if (prop == QStringLiteral("start")) {
                    bool ok = false;
                    double ms = val.toDouble(&ok);
                    if (ok) rawMap[index].start = ms / 1000.0;
                }
            }
        }
    }

    for (auto it = rawMap.begin(); it != rawMap.end(); ++it) {
        if (!it.value().file.isEmpty()) {
            PlaylistItem item;
            item.filePath = it.value().file;
            item.title = it.value().title.isEmpty() ? QFileInfo(item.filePath).fileName() : it.value().title;
            item.duration = it.value().duration;
            item.startOffset = it.value().start;
            items.append(item);
        }
    }

    return items;
}

bool DplParser::save(const QString &filePath, const QList<PlaylistItem> &items, const QString &currentPlaying, double playtime) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out << "DAUMPLAYLIST\n";
    out << "playname=" << currentPlaying << "\n";
    out << "playtime=" << static_cast<qint64>(playtime * 1000.0) << "\n";
    out << "topindex=0\n";
    out << "saveplaypos=0\n";

    for (int i = 0; i < items.size(); ++i) {
        int index = i + 1;
        const auto &item = items.at(i);
        out << index << "*file*" << item.filePath << "\n";
        if (!item.title.isEmpty() && item.title != QFileInfo(item.filePath).fileName()) {
            out << index << "*title*" << item.title << "\n";
        }
        if (item.duration > 0.0) {
            out << index << "*duration2*" << static_cast<qint64>(item.duration * 1000.0) << "\n";
        }
        if (item.startOffset > 0.0) {
            out << index << "*start*" << static_cast<qint64>(item.startOffset * 1000.0) << "\n";
        }
    }

    return true;
}

} // namespace Omnipot
