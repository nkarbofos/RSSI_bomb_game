#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace rssi_game::client {

/// TCP-клиент протокола: I/O в отдельном потоке (`io_context::run`), сигналы в GUI-поток.
class NetworkClient : public QObject {
    Q_OBJECT

public:
    explicit NetworkClient(QObject* parent = nullptr);
    ~NetworkClient() override;

public slots:
    void connectToServer(QString host, quint16 port);
    void disconnectClient();

    void createLobby();
    void joinLobby(QString inviteCode);
    void placeTransmitters(int x1, int y1, int x2, int y2, int x3, int y3);
    void endPlacement();
    void sendGradientStep();
    void moveReceiver(int idx, int x, int y);
    void addReceiver(int x, int y);
    void removeReceiver(int idx);
    void sendDone();

signals:
    void roleReceived(QString role);
    void inviteReceived(QString code);
    void gameStarted(int gridW, int gridH, int placementSec);
    void placementDone(QString line);
    void stateLineReceived(QString line);
    void gameFinished(QString line);
    void errorReceived(QString message);
    void connectionChanged(bool connected);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rssi_game::client
