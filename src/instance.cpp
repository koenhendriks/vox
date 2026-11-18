#include "instance.h"

#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QStandardPaths>
#include <unistd.h>

Instance::Instance(const QString &socketPath, QObject *parent)
    : QObject(parent), m_path(socketPath.isEmpty() ? defaultSocketPath() : socketPath) {
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                while (socket->canReadLine())
                    emit command(QString::fromUtf8(socket->readLine()).trimmed());
            });
        }
    });
}

QString Instance::defaultSocketPath() {
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtime.isEmpty())
        return runtime + "/vox/instance.sock";
    return QDir::tempPath() + QString("/vox-%1/instance.sock").arg(getuid());
}

bool Instance::send(const QString &command) const {
    QLocalSocket socket;
    socket.connectToServer(m_path);
    if (!socket.waitForConnected(500))
        return false;
    socket.write(command.toUtf8() + '\n');
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

bool Instance::listen() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // A socket left by a Vox that crashed refuses connections; send() has
    // already found nobody listening, so it's safe to take over.
    QLocalServer::removeServer(m_path);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    return m_server.listen(m_path);
}
