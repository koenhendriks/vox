#pragma once

#include <QObject>
#include <QUrl>

// Draws the tray icon for each state in the theme's accent and saves it where
// the tray can load it. A tray icon is exported to the panel as an image, so
// following a theme switch means drawing a new one.
class TrayIcons : public QObject {
    Q_OBJECT

public:
    explicit TrayIcons(const QString &directory = {}, QObject *parent = nullptr);

    // `state` is "on", "off" or "compare"; the file name carries the state
    // and color, so a change is a new URL and the tray reloads it.
    Q_INVOKABLE QUrl source(const QString &state, const QString &accent);

private:
    QString m_directory;
};
