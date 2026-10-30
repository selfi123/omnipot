#include "subtitle/whisper_model.h"
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

namespace Omnipot::WhisperModels {

// Files below this size are broken or partial downloads
static constexpr qint64 kMinModelBytes = 30 * 1024 * 1024;

QList<WhisperModelInfo> all() {
    return {
        {QStringLiteral("tiny"), QStringLiteral("Tiny (fastest)"), 75},
        {QStringLiteral("base"), QStringLiteral("Base (balanced)"), 142},
        {QStringLiteral("small"), QStringLiteral("Small (most accurate)"), 466},
    };
}

WhisperModelInfo info(const QString &id) {
    const auto models = all();
    for (const WhisperModelInfo &m : models) {
        if (m.id == id) return m;
    }
    return models.first();
}

QString autoSelect(int cpuThreads) {
    if (cpuThreads < 4) return QStringLiteral("tiny");
    if (cpuThreads < 8) return QStringLiteral("base");
    return QStringLiteral("small");
}

QString resolve(const QString &setting, int cpuThreads) {
    const auto models = all();
    const bool known = std::any_of(models.cbegin(), models.cend(), [&](const WhisperModelInfo &m) { return m.id == setting; });
    return known ? setting : autoSelect(cpuThreads);
}

int recommendedThreads(int cpuThreads) {
    return std::clamp(cpuThreads / 2, 1, 4);
}

QString modelDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/omnipot/models");
}

QString modelPath(const QString &id) {
    return modelDirectory() + QStringLiteral("/ggml-") + id + QStringLiteral(".bin");
}

bool isInstalled(const QString &id) {
    const QFileInfo fi(modelPath(id));
    return fi.isFile() && fi.size() >= kMinModelBytes;
}

QUrl downloadUrl(const QString &id) {
    return QUrl(QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-%1.bin").arg(id));
}

QList<QPair<QString, QString>> languages() {
    return {
        {QStringLiteral("auto"), QStringLiteral("Detect automatically")},
        {QStringLiteral("ar"), QStringLiteral("Arabic")},
        {QStringLiteral("bn"), QStringLiteral("Bengali")},
        {QStringLiteral("zh"), QStringLiteral("Chinese")},
        {QStringLiteral("nl"), QStringLiteral("Dutch")},
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("fr"), QStringLiteral("French")},
        {QStringLiteral("de"), QStringLiteral("German")},
        {QStringLiteral("hi"), QStringLiteral("Hindi")},
        {QStringLiteral("id"), QStringLiteral("Indonesian")},
        {QStringLiteral("it"), QStringLiteral("Italian")},
        {QStringLiteral("ja"), QStringLiteral("Japanese")},
        {QStringLiteral("ko"), QStringLiteral("Korean")},
        {QStringLiteral("ml"), QStringLiteral("Malayalam")},
        {QStringLiteral("pl"), QStringLiteral("Polish")},
        {QStringLiteral("pt"), QStringLiteral("Portuguese")},
        {QStringLiteral("ru"), QStringLiteral("Russian")},
        {QStringLiteral("es"), QStringLiteral("Spanish")},
        {QStringLiteral("ta"), QStringLiteral("Tamil")},
        {QStringLiteral("te"), QStringLiteral("Telugu")},
        {QStringLiteral("th"), QStringLiteral("Thai")},
        {QStringLiteral("tr"), QStringLiteral("Turkish")},
        {QStringLiteral("uk"), QStringLiteral("Ukrainian")},
        {QStringLiteral("vi"), QStringLiteral("Vietnamese")},
    };
}

} // namespace Omnipot::WhisperModels
