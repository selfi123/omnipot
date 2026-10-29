#pragma once

#include <QString>

namespace Omnipot {

struct PlaylistItem {
    QString filePath;
    QString title;
    double duration = 0.0; // in seconds
    double startOffset = 0.0; // in seconds

    bool isValid() const {
        return !filePath.isEmpty();
    }
};

enum class RepeatMode {
    None,
    Track,
    All
};

} // namespace Omnipot
