#pragma once

#include <QObject>
#include <QStringList>
#include <QLocalServer>
#include <QLocalSocket>

namespace Omnipot {

class MainWindow;

class SingleInstanceController : public QObject {
    Q_OBJECT

public:
    explicit SingleInstanceController(QObject *parent = nullptr);
    ~SingleInstanceController() override;

    // Checks if another instance is running. If so, sends args and returns true.
    bool sendArgsToExistingInstance(const QStringList &args);

    // Starts listening for subsequent instances
    bool startServer(MainWindow *mainWindow);

private Q_SLOTS:
    void onNewConnection();

private:
    static QString socketServerName();

    QLocalServer *m_server = nullptr;
    MainWindow *m_mainWindow = nullptr;
};

} // namespace Omnipot
