#include "trayicon.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

TrayIcons::TrayIcons(const QString &directory, QObject *parent) : QObject(parent), m_directory(directory) {
    if (m_directory.isEmpty()) {
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        m_directory = (runtime.isEmpty() ? QDir::tempPath() : runtime) + "/vox";
    }
}

QUrl TrayIcons::source(const QString &state, const QString &accent) {
    const QColor color = state == "on" ? QColor(accent) : state == "compare" ? QColor("#d0a030") : QColor("#9b9ba3");
    // Icons outlive the process in the runtime directory, so the number in
    // the name changes whenever the drawing does.
    const QString path = QString("%1/tray2-%2-%3.png").arg(m_directory, state, color.name().mid(1));
    if (QFileInfo::exists(path))
        return QUrl::fromLocalFile(path);

    // Drawn at 64 px on a transparent square; the panel scales it down.
    QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    // Comparing draws the mic hollow, so it differs from "on" by shape and
    // not only by a color that can sit close to the accent.
    if (state == "compare") {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(24.5, 7.5, 15, 29), 7.5, 7.5);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(22, 5, 20, 34), 10, 10);
    }

    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPainterPath arc;
    arc.arcMoveTo(QRectF(12, 8, 40, 40), 180);
    arc.arcTo(QRectF(12, 8, 40, 40), 180, 180);
    painter.drawPath(arc);
    painter.drawLine(QPointF(32, 48), QPointF(32, 57));
    painter.drawLine(QPointF(23, 58), QPointF(41, 58));

    if (state == "off") {
        // Cut a gap around the slash first, so it reads against the mic.
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.setPen(QPen(Qt::transparent, 11, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(10, 6), QPointF(54, 58));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(pen);
        painter.drawLine(QPointF(10, 6), QPointF(54, 58));
    }
    painter.end();

    QDir().mkpath(m_directory);
    if (!image.save(path))
        return {};
    return QUrl::fromLocalFile(path);
}
