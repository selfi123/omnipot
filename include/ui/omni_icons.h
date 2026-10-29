#pragma once

#include <QIcon>
#include <QColor>
#include <QSize>
#include <functional>

namespace Omnipot {

class OmniIcons {
public:
    static QIcon playIcon(int size = 20);
    static QIcon pauseIcon(int size = 20);
    static QIcon stopIcon(int size = 20);
    static QIcon prevIcon(int size = 20);
    static QIcon nextIcon(int size = 20);
    static QIcon openIcon(int size = 20);
    static QIcon volumeIcon(int volume, bool muted, int size = 20);
    static QIcon playlistIcon(int size = 20);
    static QIcon controlBoxIcon(int size = 20);
    static QIcon fullscreenIcon(bool isFullscreen, int size = 20);
    static QIcon pinIcon(bool isPinned, int size = 20);

private:
    using DrawFunc = std::function<void(class QPainter &, const class QRectF &, const QColor &)>;
    static QPixmap renderPixmap(int size, const DrawFunc &drawFunc, const QColor &color);
    static QIcon buildIcon(int size, const DrawFunc &drawFunc,
                           const QColor &normalColor = QColor(0xD8, 0xD8, 0xD8),
                           const QColor &hoverColor = QColor(0xFF, 0xFF, 0xFF));
};

} // namespace Omnipot

