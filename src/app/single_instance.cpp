#include "app/single_instance.h"
#include "ui/main_window.h"
#include <QDataStream>
#include <QFileInfo>
#include <QCoreApplication>

namespace PotPlayer {

QString SingleInstanceController::socketServerName() {
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty()) user = QStringLiteral("default");
    return QStringLiteral("potplayer_linux_ipc_") + user;
}

SingleInstanceController::SingleInstanceController(QObject *parent)
    : QObject(parent)
{
}

SingleInstanceController::~SingleInstanceController() {
    if (m_server) {
        m_server->close();
    }
}

bool SingleInstanceController::sendArgsToExistingInstance(const QStringList &args) {
    QLocalSocket socket;
    socket.connectToServer(socketServerName());
    if (!socket.waitForConnected(500)) {
        return false;
    }

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << args;

    socket.write(data);
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstanceController::startServer(MainWindow *mainWindow) {
    m_mainWindow = mainWindow;
    m_server = new QLocalServer(this);

    // Remove any stale socket file left from previous crashes
    QLocalServer::removeServer(socketServerName());

    if (!m_server->listen(socketServerName())) {
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceController::onNewConnection);
    return true;
}

void SingleInstanceController::onNewConnection() {
    QLocalSocket *clientSocket = m_server->nextPendingConnection();
    if (!clientSocket) return;

    connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
        QByteArray data = clientSocket->readAll();
        QDataStream in(&data, QIODevice::ReadOnly);
        QStringList args;
        in >> args;

        if (m_mainWindow && !args.isEmpty()) {
            for (const QString &arg : args) {
                if (arg == QStringLiteral("-")) {
                    m_mainWindow->playlistManager()->addPath(QStringLiteral("fd://0"));
                } else if (QFileInfo::exists(arg) || arg.contains(QStringLiteral("://"))) {
                    m_mainWindow->playlistManager()->addPath(arg);
                }
            }

            // Play the last added or open directly
            if (!args.isEmpty()) {
                QString first = args.first();
                if (first == QStringLiteral("-")) {
                    m_mainWindow->openMediaFile(QStringLiteral("fd://0"));
                } else if (QFileInfo::exists(first) || first.contains(QStringLiteral("://"))) {
                    m_mainWindow->openMediaFile(first);
                }
            }

            m_mainWindow->showNormal();
            m_mainWindow->raise();
            m_mainWindow->activateWindow();
        }

        clientSocket->disconnectFromServer();
    });

    connect(clientSocket, &QLocalSocket::disconnected, clientSocket, &QLocalSocket::deleteLater);
}

} // namespace PotPlayer
