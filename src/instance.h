#pragma once

#include <QLocalServer>
#include <QObject>

// Keeps Vox to one process per user, so the window and the tray icon belong
// together. A second `vox` or `vox tray` hands its command to the first.
class Instance : public QObject {
    Q_OBJECT

public:
    explicit Instance(const QString &socketPath = {}, QObject *parent = nullptr);

    // Delivers `command` to a running Vox; false when there is none.
    bool send(const QString &command) const;
    bool listen();

    static QString defaultSocketPath();

signals:
    void command(const QString &command);

private:
    QString m_path;
    QLocalServer m_server;
};
