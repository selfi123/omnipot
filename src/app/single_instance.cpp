#include "app/single_instance.h"
#include "ui/main_window.h"
#include <QDataStream>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>

namespace Omnipot {

QString SingleInstanceController::socketServerName() {
    // Prefer the per-user runtime dir (mode 0700) over the shared /tmp, where
    // another local user could squat on a predictable socket name
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtimeDir.isEmpty()) {
        return runtimeDir + QStringLiteral("/omnipot.sock");
    }
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty()) user = QStringLiteral("default");
    return QStringLiteral("omnipot_ipc_") + user;
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
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

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
        // The message may arrive in several chunks; wait until it is complete
        QDataStream in(clientSocket);
        in.startTransaction();
        QStringList args;
        in >> args;
        if (!in.commitTransaction()) {
            return;
        }

        if (m_mainWindow) {
            QStringList paths;
            for (const QString &arg : std::as_const(args)) {
                // stdin belongs to the forwarding process, not to this one
                if (arg != QStringLiteral("-")) {
                    paths.append(arg);
                }
            }
            m_mainWindow->openPaths(paths);

            m_mainWindow->showNormal();
            m_mainWindow->raise();
            m_mainWindow->activateWindow();
        }

        clientSocket->disconnectFromServer();
    });

    connect(clientSocket, &QLocalSocket::disconnected, clientSocket, &QLocalSocket::deleteLater);
}

} // namespace Omnipot
