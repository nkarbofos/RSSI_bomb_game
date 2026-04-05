#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

#include <memory>
#include <utility>

#include "common/protocol_parser.hpp"

class QTimer;

namespace rssi_game::client {

class NetworkClient;

/// Связка QML ↔ NetworkClient: свойства поля, RSSI, фаза и роль.
class GameController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString inviteCode READ inviteCode NOTIFY inviteCodeChanged)
    Q_PROPERTY(QString myRole READ myRole NOTIFY myRoleChanged)
    Q_PROPERTY(int gridW READ gridW NOTIFY gridChanged)
    Q_PROPERTY(int gridH READ gridH NOTIFY gridChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool boardVisible READ boardVisible NOTIFY boardVisibleChanged)
    Q_PROPERTY(int placementSecondsRemaining READ placementSecondsRemaining NOTIFY
                     placementSecondsRemainingChanged)
    Q_PROPERTY(QVariantList cellColors READ cellColors NOTIFY cellColorsChanged)
    Q_PROPERTY(QString lastStateLine READ lastStateLine NOTIFY lastStateLineChanged)
    Q_PROPERTY(QVariantList placementMask READ placementMask NOTIFY placementMaskChanged)
    Q_PROPERTY(int placementSelectionCount READ placementSelectionCount NOTIFY
                     placementSelectionCountChanged)
    Q_PROPERTY(int selectedCellIndex READ selectedCellIndex NOTIFY selectedCellIndexChanged)

public:
    explicit GameController(QObject* parent = nullptr);
    ~GameController() override;

    QString host() const { return host_; }
    void setHost(const QString& h);

    int port() const { return port_; }
    void setPort(int p);

    QString inviteCode() const { return invite_; }
    QString myRole() const { return my_role_; }
    int gridW() const { return grid_w_; }
    int gridH() const { return grid_h_; }
    QString phase() const { return phase_; }
    QString status() const { return status_; }
    bool boardVisible() const { return board_visible_; }
    int placementSecondsRemaining() const { return placement_seconds_remaining_; }
    QVariantList cellColors() const { return cell_colors_; }
    QString lastStateLine() const { return last_state_; }
    QVariantList placementMask() const { return placement_mask_; }
    int placementSelectionCount() const { return placement_pick_.size(); }
    int selectedCellIndex() const { return selected_cell_index_; }

public slots:
    void connectToServer();
    void disconnectFromServer();
    void createLobby();
    void joinLobby(const QString& code);
    void placeTransmitters(int x1, int y1, int x2, int y2, int x3, int y3);
    void endPlacement();
    void sendGradientStep();
    void sendDone();
    Q_INVOKABLE void hiderClickCell(int cellIndex);
    Q_INVOKABLE void confirmPlacementFromSelection();
    Q_INVOKABLE void seekerClickCell(int cellIndex);
    Q_INVOKABLE void moveReceiverToSelected(int receiverIndex);
    Q_INVOKABLE void addReceiverAtSelected();
    Q_INVOKABLE void removeReceiverSlot(int receiverIndex);

signals:
    void hostChanged();
    void portChanged();
    void inviteCodeChanged();
    void myRoleChanged();
    void gridChanged();
    void phaseChanged();
    void statusChanged();
    void boardVisibleChanged();
    void placementSecondsRemainingChanged();
    void cellColorsChanged();
    void lastStateLineChanged();
    void placementMaskChanged();
    void placementSelectionCountChanged();
    void selectedCellIndexChanged();

private slots:
    void onRoleReceived(const QString& role);
    void onInvite(const QString& code);
    void onGameStarted(int w, int h, int placementSec);
    void onStateLine(const QString& line);
    void onPlacementDone(const QString& line);
    void onGameFinished(const QString& line);
    void onError(const QString& msg);
    void onConnectionChanged(bool connected);
    void tickPlacementTimer();

private:
    void setStatus(const QString& s);
    void rebuildCellColors();
    void refreshPlacementGridColors();
    void clearPlacementSelection();
    bool isHider() const;
    bool isSeeker() const;
    bool isPlacementPhase() const;
    bool isSearchPhase() const;
    void resetSessionUi();
    void updatePlacementRemaining();
    void stopPlacementTimer();

    std::unique_ptr<NetworkClient> net_;
    QTimer* placement_timer_{nullptr};

    QString host_{"127.0.0.1"};
    int port_{3000};
    QString invite_;
    QString my_role_;
    int grid_w_{10};
    int grid_h_{10};
    QString phase_{QStringLiteral("Idle")};
    QString status_{};
    bool board_visible_{false};
    bool placement_done_received_{false};
    int placement_seconds_remaining_{0};
    QDateTime placement_end_;

    QVariantList cell_colors_;
    QString last_state_;

    rssi_game::protocol::StateData last_state_data_{};

    QVector<std::pair<int, int>> placement_pick_;
    QVariantList placement_mask_;

    int selected_cell_index_{-1};
    QVector<std::pair<int, int>> result_tx_positions_;
};

} // namespace rssi_game::client
