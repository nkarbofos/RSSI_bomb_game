#include "client/game_controller.hpp"

#include "client/network_client.hpp"

#include <QColor>
#include <algorithm>
#include <cmath>

namespace rssi_game::client {

namespace {

QString rssiToColor(double avgDbm) {
    const double lo = -100.0;
    const double hi = -30.0;
    double t = (avgDbm - lo) / (hi - lo);
    t = std::max(0.0, std::min(1.0, t));
    const int hue = static_cast<int>(240.0 - t * 240.0);
    return QColor::fromHsv(hue, 220, 230).name(QColor::HexRgb);
}

double rowAverage(const std::vector<double>& row) {
    if (row.empty()) {
        return -80.0;
    }
    double s = 0;
    for (double v : row) {
        s += v;
    }
    return s / static_cast<double>(row.size());
}

} // namespace

GameController::GameController(QObject* parent) : QObject(parent), net_(std::make_unique<NetworkClient>()) {
    connect(net_.get(), &NetworkClient::inviteReceived, this, &GameController::onInvite);
    connect(net_.get(), &NetworkClient::gameStarted, this, &GameController::onGameStarted);
    connect(net_.get(), &NetworkClient::stateLineReceived, this, &GameController::onStateLine);
    connect(net_.get(), &NetworkClient::placementDone, this, &GameController::onPlacementDone);
    connect(net_.get(), &NetworkClient::gameFinished, this, &GameController::onGameFinished);
    connect(net_.get(), &NetworkClient::errorReceived, this, &GameController::onError);
    connect(net_.get(), &NetworkClient::connectionChanged, this, &GameController::onConnectionChanged);
}

GameController::~GameController() = default;

void GameController::setHost(const QString& h) {
    if (host_ == h) {
        return;
    }
    host_ = h;
    emit hostChanged();
}

void GameController::setPort(int p) {
    if (port_ == p) {
        return;
    }
    port_ = p;
    emit portChanged();
}

void GameController::setStatus(const QString& s) {
    if (status_ == s) {
        return;
    }
    status_ = s;
    emit statusChanged();
}

void GameController::connectToServer() {
    setStatus(QStringLiteral("Connecting…"));
    net_->connectToServer(host_, static_cast<quint16>(std::max(1, std::min(65535, port_))));
}

void GameController::disconnectFromServer() {
    net_->disconnectClient();
    setStatus(QStringLiteral("Disconnected"));
    phase_ = QStringLiteral("Idle");
    emit phaseChanged();
}

void GameController::createLobby() {
    net_->createLobby();
}

void GameController::joinLobby(const QString& code) {
    net_->joinLobby(code);
}

void GameController::placeTransmitters(int x1, int y1, int x2, int y2, int x3, int y3) {
    net_->placeTransmitters(x1, y1, x2, y2, x3, y3);
}

void GameController::sendGradientStep() {
    net_->sendGradientStep();
}

void GameController::sendDone() {
    net_->sendDone();
}

void GameController::onInvite(const QString& code) {
    invite_ = code;
    emit inviteCodeChanged();
    phase_ = QStringLiteral("Lobby");
    emit phaseChanged();
    setStatus(QStringLiteral("Invite code: ") + code);
}

void GameController::onGameStarted(int w, int h, int /*placementSec*/) {
    grid_w_ = w;
    grid_h_ = h;
    emit gridChanged();
    phase_ = QStringLiteral("Placement");
    emit phaseChanged();
    setStatus(QStringLiteral("GAME_START ") + QString::number(w) + QLatin1Char('x') + QString::number(h));
}

void GameController::onStateLine(const QString& line) {
    last_state_ = line;
    emit lastStateLineChanged();

    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(line.toStdString(), p);
    if (p.kind == rssi_game::protocol::ServerLineKind::State && p.state) {
        last_state_data_ = *p.state;
        phase_ = QStringLiteral("Search");
        emit phaseChanged();
        rebuildCellColors();
    }
}

void GameController::onPlacementDone(const QString& line) {
    setStatus(line);
    phase_ = QStringLiteral("Search");
    emit phaseChanged();
}

void GameController::onGameFinished(const QString& line) {
    setStatus(line);
    phase_ = QStringLiteral("Finished");
    emit phaseChanged();
}

void GameController::onError(const QString& msg) {
    setStatus(QStringLiteral("ERROR: ") + msg);
}

void GameController::onConnectionChanged(bool connected) {
    if (!connected) {
        setStatus(QStringLiteral("Not connected"));
    } else {
        setStatus(QStringLiteral("Connected"));
    }
}

void GameController::rebuildCellColors() {
    QVariantList out;
    out.reserve(grid_w_ * grid_h_);
    const QString neutral = QStringLiteral("#eeeeee");

    for (int y = 0; y < grid_h_; ++y) {
        for (int x = 0; x < grid_w_; ++x) {
            QString color = neutral;
            for (std::size_t i = 0; i < last_state_data_.receiver_positions.size(); ++i) {
                const auto& pos = last_state_data_.receiver_positions[i];
                if (pos.first == x && pos.second == y) {
                    double avg = -75.0;
                    if (i < last_state_data_.rssi.size()) {
                        avg = rowAverage(last_state_data_.rssi[i]);
                    }
                    color = rssiToColor(avg);
                    break;
                }
            }
            out.push_back(color);
        }
    }
    cell_colors_ = std::move(out);
    emit cellColorsChanged();
}

} // namespace rssi_game::client
