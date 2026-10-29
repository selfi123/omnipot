#include "ui/omni_icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <cmath>

namespace Omnipot {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

QPixmap OmniIcons::renderPixmap(int size, const DrawFunc &drawFunc, const QColor &color) {
    const int scale = 2; // Supersample 2x for sharp rendering
    QPixmap pix(size * scale, size * scale);
    pix.fill(Qt::transparent);
    pix.setDevicePixelRatio(scale);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QRectF r(0, 0, size, size);
    drawFunc(p, r, color);
    p.end();

    return pix;
}

QIcon OmniIcons::buildIcon(int size, const DrawFunc &drawFunc,
                          const QColor &normalColor,
                          const QColor &hoverColor) {
    QIcon icon;
    icon.addPixmap(renderPixmap(size, drawFunc, normalColor), QIcon::Normal);
    icon.addPixmap(renderPixmap(size, drawFunc, hoverColor), QIcon::Active);
    icon.addPixmap(renderPixmap(size, drawFunc, QColor(0x66, 0x66, 0x66)), QIcon::Disabled);
    icon.addPixmap(renderPixmap(size, drawFunc, QColor(0xAA, 0xAA, 0xAA)), QIcon::Selected);
    return icon;
}

QIcon OmniIcons::playIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        QPolygonF poly({
            QPointF(s * 0.33, s * 0.23),
            QPointF(s * 0.77, s * 0.50),
            QPointF(s * 0.33, s * 0.77)
        });
        p.drawPolygon(poly);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::pauseIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        qreal barW = s * 0.16;
        qreal barH = s * 0.52;
        qreal y = (s - barH) / 2.0;
        p.drawRoundedRect(QRectF(s * 0.28, y, barW, barH), 1, 1);
        p.drawRoundedRect(QRectF(s * 0.56, y, barW, barH), 1, 1);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::stopIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        qreal sqSize = s * 0.48;
        qreal x = (s - sqSize) / 2.0;
        qreal y = (s - sqSize) / 2.0;
        p.drawRoundedRect(QRectF(x, y, sqSize, sqSize), 1, 1);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::prevIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        // Bar on left
        p.drawRoundedRect(QRectF(s * 0.22, s * 0.26, s * 0.11, s * 0.48), 0.5, 0.5);
        // Triangle pointing left
        QPolygonF poly({
            QPointF(s * 0.76, s * 0.26),
            QPointF(s * 0.38, s * 0.50),
            QPointF(s * 0.76, s * 0.74)
        });
        p.drawPolygon(poly);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::nextIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        // Triangle pointing right
        QPolygonF poly({
            QPointF(s * 0.24, s * 0.26),
            QPointF(s * 0.62, s * 0.50),
            QPointF(s * 0.24, s * 0.74)
        });
        p.drawPolygon(poly);
        // Bar on right
        p.drawRoundedRect(QRectF(s * 0.67, s * 0.26, s * 0.11, s * 0.48), 0.5, 0.5);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::openIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        // Upward pointing triangle
        QPolygonF poly({
            QPointF(s * 0.50, s * 0.23),
            QPointF(s * 0.20, s * 0.56),
            QPointF(s * 0.80, s * 0.56)
        });
        p.drawPolygon(poly);
        // Horizontal bar
        p.drawRoundedRect(QRectF(s * 0.20, s * 0.66, s * 0.60, s * 0.12), 0.5, 0.5);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::volumeIcon(int volume, bool muted, int size) {
    auto draw = [volume, muted](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();

        // Speaker box
        p.drawRoundedRect(QRectF(s * 0.16, s * 0.38, s * 0.15, s * 0.24), 0.5, 0.5);

        // Speaker cone
        QPolygonF cone({
            QPointF(s * 0.29, s * 0.38),
            QPointF(s * 0.50, s * 0.22),
            QPointF(s * 0.50, s * 0.78),
            QPointF(s * 0.29, s * 0.62)
        });
        p.drawPolygon(cone);

        if (muted || volume == 0) {
            // Mute cross 'X'
            QPen pen(muted ? QColor(0xEE, 0x44, 0x44) : c, s * 0.08, Qt::SolidLine, Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(QPointF(s * 0.62, s * 0.38), QPointF(s * 0.84, s * 0.62));
            p.drawLine(QPointF(s * 0.84, s * 0.38), QPointF(s * 0.62, s * 0.62));
        } else {
            // Sound wave arcs
            QPen arcPen(c, s * 0.08, Qt::SolidLine, Qt::RoundCap);
            p.setPen(arcPen);
            p.setBrush(Qt::NoBrush);

            QRectF arc1(s * 0.40, s * 0.34, s * 0.24, s * 0.32);
            p.drawArc(arc1, -50 * 16, 100 * 16);

            if (volume > 60) {
                QRectF arc2(s * 0.36, s * 0.22, s * 0.44, s * 0.56);
                p.drawArc(arc2, -50 * 16, 100 * 16);
            }
        }
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::playlistIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        qreal s = r.width();
        qreal lineH = s * 0.10;
        qreal lineW = s * 0.58;
        qreal x = (s - lineW) / 2.0;
        p.drawRoundedRect(QRectF(x, s * 0.26, lineW, lineH), 0.5, 0.5);
        p.drawRoundedRect(QRectF(x, s * 0.45, lineW, lineH), 0.5, 0.5);
        p.drawRoundedRect(QRectF(x, s * 0.64, lineW, lineH), 0.5, 0.5);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::controlBoxIcon(int size) {
    auto draw = [](QPainter &p, const QRectF &r, const QColor &c) {
        qreal s = r.width();
        QPointF center(s / 2.0, s / 2.0);
        qreal outerR = s * 0.36;
        qreal innerR = s * 0.27;
        qreal holeR = s * 0.13;
        const int teeth = 6;

        QPainterPath path;
        for (int i = 0; i < teeth; ++i) {
            double a1 = (i * 2.0 * M_PI) / teeth;
            double a2 = a1 + (0.45 * M_PI) / teeth;
            double a3 = a1 + (1.00 * M_PI) / teeth;
            double a4 = a1 + (1.45 * M_PI) / teeth;

            QPointF p1 = center + QPointF(outerR * std::cos(a1), outerR * std::sin(a1));
            QPointF p2 = center + QPointF(outerR * std::cos(a2), outerR * std::sin(a2));
            QPointF p3 = center + QPointF(innerR * std::cos(a3), innerR * std::sin(a3));
            QPointF p4 = center + QPointF(innerR * std::cos(a4), innerR * std::sin(a4));

            if (i == 0) path.moveTo(p1);
            else path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(p3);
            path.lineTo(p4);
        }
        path.closeSubpath();

        QPainterPath hole;
        hole.addEllipse(center, holeR, holeR);
        QPainterPath gear = path.subtracted(hole);

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(gear);
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::fullscreenIcon(bool isFullscreen, int size) {
    auto draw = [isFullscreen](QPainter &p, const QRectF &r, const QColor &c) {
        qreal s = r.width();
        QPen pen(c, s * 0.09, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        if (!isFullscreen) {
            // Expand corners
            qreal m = s * 0.25;
            qreal len = s * 0.18;
            // Top-Left
            p.drawLine(QPointF(m, m + len), QPointF(m, m));
            p.drawLine(QPointF(m, m), QPointF(m + len, m));
            // Top-Right
            p.drawLine(QPointF(s - m - len, m), QPointF(s - m, m));
            p.drawLine(QPointF(s - m, m), QPointF(s - m, m + len));
            // Bottom-Left
            p.drawLine(QPointF(m, s - m - len), QPointF(m, s - m));
            p.drawLine(QPointF(m, s - m), QPointF(m + len, s - m));
            // Bottom-Right
            p.drawLine(QPointF(s - m - len, s - m), QPointF(s - m, s - m));
            p.drawLine(QPointF(s - m, s - m), QPointF(s - m, s - m - len));
        } else {
            // Collapse corners
            qreal m = s * 0.22;
            qreal cOff = s * 0.40;
            // Top-Left corner aiming inward
            p.drawLine(QPointF(m, cOff), QPointF(cOff, cOff));
            p.drawLine(QPointF(cOff, cOff), QPointF(cOff, m));
            // Top-Right
            p.drawLine(QPointF(s - m, cOff), QPointF(s - cOff, cOff));
            p.drawLine(QPointF(s - cOff, cOff), QPointF(s - cOff, m));
            // Bottom-Left
            p.drawLine(QPointF(m, s - cOff), QPointF(cOff, s - cOff));
            p.drawLine(QPointF(cOff, s - cOff), QPointF(cOff, s - m));
            // Bottom-Right
            p.drawLine(QPointF(s - m, s - cOff), QPointF(s - cOff, s - cOff));
            p.drawLine(QPointF(s - cOff, s - cOff), QPointF(s - cOff, s - m));
        }
    };
    return buildIcon(size, draw);
}

QIcon OmniIcons::pinIcon(bool isPinned, int size) {
    auto draw = [isPinned](QPainter &p, const QRectF &r, const QColor &c) {
        qreal s = r.width();
        QColor color = isPinned ? QColor(0xF0, 0xA0, 0x20) : c;
        p.setPen(Qt::NoPen);
        p.setBrush(color);

        // Pin head circle
        p.drawEllipse(QPointF(s * 0.64, s * 0.30), s * 0.10, s * 0.10);

        // Diagonal body
        QPolygonF body({
            QPointF(s * 0.58, s * 0.24),
            QPointF(s * 0.76, s * 0.42),
            QPointF(s * 0.50, s * 0.64),
            QPointF(s * 0.36, s * 0.50)
        });
        p.drawPolygon(body);

        // Crossbar
        p.drawRoundedRect(QRectF(s * 0.29, s * 0.58, s * 0.28, s * 0.08), 0.5, 0.5);

        // Needle tip
        QPen needlePen(color, s * 0.08, Qt::SolidLine, Qt::RoundCap);
        p.setPen(needlePen);
        p.drawLine(QPointF(s * 0.38, s * 0.62), QPointF(s * 0.22, s * 0.78));
    };
    return buildIcon(size, draw);
}

} // namespace Omnipot

