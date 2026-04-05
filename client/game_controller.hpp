#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

#include <memory>

#include "common/protocol_parser.hpp"

namespace rssi_game::client {

class NetworkClient;

/// Связка QML ↔ NetworkClient: свойства поля, RSSI и фаза игры.
class GameController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString inviteCode READ inviteCode NOTIFY inviteCodeChanged)
    Q_PROPERTY(int gridW READ gridW NOTIFY gridChanged)
    Q_PROPERTY(int gridH READ gridH NOTIFY gridChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList cellColors READ cellColors NOTIFY cellColorsChanged)
    Q_PROPERTY(QString lastStateLine READ lastStateLine NOTIFY lastStateLineChanged)

public:
    explicit GameController(QObject* parent = nullptr);
    ~GameController() override;

    QString host() const { return host_; }
    void setHost(const QString& h);

    int port() const { return port_; }
    void setPort(int p);

    QString inviteCode() const { return invite_; }
    int gridW() const { return grid_w_; }
    int gridH() const { return grid_h_; }
    QString phase() const { return phase_; }
    QString status() const { return status_; }
    QVariantList cellColors() const { return cell_colors_; }
    QString lastStateLine() const { return last_state_; }

public slots:
    void connectToServer();
    void disconnectFromServer();
    void createLobby();
    void joinLobby(const QString& code);
    void placeTransmitters(int x1, int y1, int x2, int y2, int x3, int y3);
    void sendGradientStep();
    void sendDone();

signals:
    void hostChanged();
    void portChanged();
    void inviteCodeChanged();
    void gridChanged();
    void phaseChanged();
    void statusChanged();
    void cellColorsChanged();
    void lastStateLineChanged();

private:
    void setStatus(const QString& s);
    void onInvite(const QString& code);
    void onGameStarted(int w, int h, int placementSec);
    void onStateLine(const QString& line);
    void onPlacementDone(const QString& line);
    void onGameFinished(const QString& line);
    void onError(const QString& msg);
    void onConnectionChanged(bool connected);
    void rebuildCellColors();

    std::unique_ptr<NetworkClient> net_;

    QString host_{"127.0.0.1"};
    int port_{3000};
    QString invite_;
    int grid_w_{10};
    int grid_h_{10};
    QString phase_{"Idle"};
    QString status_{};
    QVariantList cell_colors_;
    QString last_state_;

    rssi_game::protocol::StateData last_state_data_{};
};

} // namespace rssi_game::client
