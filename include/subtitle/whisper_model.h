#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

namespace Omnipot {

struct WhisperModelInfo {
    QString id;          // "tiny", "base", "small"
    QString displayName;
    int approxSizeMb = 0;
};

// Speech models used by Realtime subtitle. Only multilingual models are offered,
// because the English-only ones cannot translate.
namespace WhisperModels {

QList<WhisperModelInfo> all();
WhisperModelInfo info(const QString &id);

// Picks a model the CPU can keep up with
QString autoSelect(int cpuThreads);
// Turns the stored setting ("auto" or a model id) into a model id
QString resolve(const QString &setting, int cpuThreads);
// Leaves half the CPU free for video decoding
int recommendedThreads(int cpuThreads);

// $XDG_DATA_HOME/omnipot/models
QString modelDirectory();
QString modelPath(const QString &id);
bool isInstalled(const QString &id);
QUrl downloadUrl(const QString &id);

// Spoken languages offered in Preferences, as (whisper code, display name)
QList<QPair<QString, QString>> languages();

} // namespace WhisperModels

} // namespace Omnipot
