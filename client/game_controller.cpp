#include "client/game_controller.hpp"

#include "client/network_client.hpp"

#include <QTimer>

#include <algorithm>
#include <utility>

namespace rssi_game::client {

GameController::GameController(QObject* parent) : QObject(parent), net_(std::make_unique<NetworkClient>()) {
    placement_timer_ = new QTimer(this);
    placement_timer_->setInterval(1000);
    connect(placement_timer_, &QTimer::timeout, this, &GameController::tickPlacementTimer);

    connect(net_.get(), &NetworkClient::roleReceived, this, &GameController::onRoleReceived);
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

bool GameController::isHider() const {
    return my_role_.compare(QStringLiteral("Hider"), Qt::CaseInsensitive) == 0;
}

bool GameController::isSeeker() const {
    return my_role_.compare(QStringLiteral("Seeker"), Qt::CaseInsensitive) == 0;
}

bool GameController::isPlacementPhase() const {
    return phase_ == QStringLiteral("Placement");
}

bool GameController::isSearchPhase() const {
    return phase_ == QStringLiteral("Search");
}

void GameController::clearPlacementSelection() {
    const int oldCount = placement_pick_.size();
    placement_pick_.clear();
    if (oldCount != 0) {
        emit placementSelectionCountChanged();
    }
    refreshPlacementGridColors();
}

void GameController::refreshPlacementGridColors() {
    const int cells = grid_w_ * grid_h_;
    QVariantList mask;
    mask.reserve(cells);
    QVariantList out;
    out.reserve(cells);

    const QString neutral = QStringLiteral("#eeeeee");
    const QString c1 = QStringLiteral("#c8e6c9");
    const QString c2 = QStringLiteral("#81c784");
    const QString c3 = QStringLiteral("#43a047");

    if (!isHider() || !isPlacementPhase()) {
        for (int i = 0; i < cells; ++i) {
            mask.push_back(QVariant(0));
            out.push_back(neutral);
        }
        placement_mask_ = std::move(mask);
        cell_colors_ = std::move(out);
        emit cellColorsChanged();
        emit placementMaskChanged();
        return;
    }

    for (int i = 0; i < cells; ++i) {
        const int x = i % grid_w_;
        const int y = i / grid_w_;
        int m = 0;
        QString color = neutral;
        for (int k = 0; k < placement_pick_.size(); ++k) {
            if (placement_pick_[k].first == x && placement_pick_[k].second == y) {
                m = k + 1;
                if (m == 1) {
                    color = c1;
                } else if (m == 2) {
                    color = c2;
                } else {
                    color = c3;
                }
                break;
            }
        }
        mask.push_back(QVariant(m));
        out.push_back(color);
    }
    placement_mask_ = std::move(mask);
    cell_colors_ = std::move(out);
    emit cellColorsChanged();
    emit placementMaskChanged();
}

void GameController::resetSessionUi() {
    stopPlacementTimer();
    board_visible_ = false;
    emit boardVisibleChanged();
    placement_done_received_ = false;
    placement_seconds_remaining_ = 0;
    emit placementSecondsRemainingChanged();
    my_role_.clear();
    emit myRoleChanged();
    invite_.clear();
    emit inviteCodeChanged();
    phase_ = QStringLiteral("Idle");
    emit phaseChanged();
    last_state_.clear();
    emit lastStateLineChanged();
    clearPlacementSelection();
    last_state_data_ = {};
    result_tx_positions_.clear();
    if (selected_cell_index_ != -1) {
        selected_cell_index_ = -1;
        emit selectedCellIndexChanged();
    }
}

void GameController::hiderClickCell(int cellIndex) {
    if (!board_visible_ || !isHider() || !isPlacementPhase()) {
        return;
    }
    const int cells = grid_w_ * grid_h_;
    if (cellIndex < 0 || cellIndex >= cells) {
        return;
    }
    const int x = cellIndex % grid_w_;
    const int y = cellIndex / grid_w_;

    for (int i = 0; i < placement_pick_.size(); ++i) {
        if (placement_pick_[i].first == x && placement_pick_[i].second == y) {
            placement_pick_.removeAt(i);
            emit placementSelectionCountChanged();
            refreshPlacementGridColors();
            return;
        }
    }
    if (placement_pick_.size() >= 3) {
        return;
    }
    placement_pick_.push_back({x, y});
    emit placementSelectionCountChanged();
    refreshPlacementGridColors();
}

void GameController::confirmPlacementFromSelection() {
    if (placement_pick_.size() != 3) {
        return;
    }
    const auto& a = placement_pick_[0];
    const auto& b = placement_pick_[1];
    const auto& c = placement_pick_[2];
    placeTransmitters(a.first, a.second, b.first, b.second, c.first, c.second);
}

void GameController::seekerClickCell(int cellIndex) {
    if (!board_visible_ || !isSeeker() || !isSearchPhase()) {
        return;
    }
    const int cells = grid_w_ * grid_h_;
    if (cellIndex < 0 || cellIndex >= cells) {
        return;
    }
    if (selected_cell_index_ == cellIndex) {
        return;
    }
    selected_cell_index_ = cellIndex;
    emit selectedCellIndexChanged();
}

void GameController::moveReceiverToSelected(int receiverIndex) {
    if (!board_visible_ || !isSeeker() || !isSearchPhase() || selected_cell_index_ < 0) {
        return;
    }
    const int x = selected_cell_index_ % grid_w_;
    const int y = selected_cell_index_ / grid_w_;
    net_->moveReceiver(receiverIndex, x, y);
}

void GameController::addReceiverAtSelected() {
    if (!board_visible_ || !isSeeker() || !isSearchPhase() || selected_cell_index_ < 0) {
        return;
    }
    const int x = selected_cell_index_ % grid_w_;
    const int y = selected_cell_index_ / grid_w_;
    net_->addReceiver(x, y);
}

void GameController::removeReceiverSlot(int receiverIndex) {
    if (!board_visible_ || !isSeeker() || !isSearchPhase()) {
        return;
    }
    net_->removeReceiver(receiverIndex);
}

void GameController::connectToServer() {
    setStatus(QStringLiteral("Connecting…"));
    net_->connectToServer(host_, static_cast<quint16>(std::max(1, std::min(65535, port_))));
}

void GameController::disconnectFromServer() {
    net_->disconnectClient();
    resetSessionUi();
    setStatus(QStringLiteral("Disconnected"));
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

void GameController::endPlacement() {
    net_->endPlacement();
}

void GameController::sendGradientStep() {
    net_->sendGradientStep();
}

void GameController::sendDone() {
    net_->sendDone();
}

void GameController::onRoleReceived(const QString& role) {
    my_role_ = role.trimmed();
    emit myRoleChanged();
}

void GameController::onInvite(const QString& code) {
    invite_ = code;
    emit inviteCodeChanged();
    phase_ = QStringLiteral("Lobby");
    emit phaseChanged();
    setStatus(QStringLiteral("Invite code: ") + code);
}

void GameController::onGameStarted(int w, int h, int placementSec) {
    grid_w_ = w;
    grid_h_ = h;
    emit gridChanged();
    board_visible_ = true;
    emit boardVisibleChanged();
    placement_done_received_ = false;
    phase_ = QStringLiteral("Placement");
    emit phaseChanged();
    placement_end_ = QDateTime::currentDateTime().addSecs(std::max(0, placementSec));
    updatePlacementRemaining();
    placement_timer_->start();
    setStatus(QStringLiteral("GAME_START %1×%2, расстановка %3 с").arg(w).arg(h).arg(placementSec));
    clearPlacementSelection();
}

void GameController::onStateLine(const QString& line) {
    last_state_ = line;
    emit lastStateLineChanged();

    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(line.toStdString(), p);
    if (p.kind != rssi_game::protocol::ServerLineKind::State || !p.state) {
        return;
    }
    if (!placement_done_received_) {
        return;
    }
    last_state_data_ = *p.state;
    phase_ = QStringLiteral("Search");
    emit phaseChanged();
    rebuildCellColors();
}

void GameController::onPlacementDone(const QString& line) {
    stopPlacementTimer();
    placement_done_received_ = true;
    placement_seconds_remaining_ = 0;
    emit placementSecondsRemainingChanged();
    if (my_role_.compare(QStringLiteral("Seeker"), Qt::CaseInsensitive) == 0) {
        setStatus(QStringLiteral("Расстановка противника завершена. Фаза поиска."));
    } else {
        setStatus(line);
    }
    phase_ = QStringLiteral("Search");
    emit phaseChanged();
    clearPlacementSelection();
    if (selected_cell_index_ != -1) {
        selected_cell_index_ = -1;
        emit selectedCellIndexChanged();
    }
}

void GameController::onGameFinished(const QString& line) {
    stopPlacementTimer();
    setStatus(line);
    result_tx_positions_.clear();
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(line.toStdString(), p);
    if (p.kind == rssi_game::protocol::ServerLineKind::GameFinished && p.game_finished) {
        const std::string& raw = p.game_finished->tx_real_raw;
        if (!raw.empty()) {
            const auto lt = raw.find('<');
            const auto gt = raw.rfind('>');
            if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                const std::string inner = raw.substr(lt + 1, gt - lt - 1);
                auto parsed = rssi_game::protocol::parseCellPositionList(inner);
                result_tx_positions_.reserve(static_cast<int>(parsed.size()));
                for (const auto& pr : parsed) {
                    result_tx_positions_.push_back({pr.first, pr.second});
                }
            }
        }
    }
    if (selected_cell_index_ != -1) {
        selected_cell_index_ = -1;
        emit selectedCellIndexChanged();
    }
    phase_ = QStringLiteral("Finished");
    emit phaseChanged();
    rebuildCellColors();
}

void GameController::onError(const QString& msg) {
    setStatus(QStringLiteral("ERROR: ") + msg);
}

void GameController::onConnectionChanged(bool connected) {
    if (!connected) {
        resetSessionUi();
        setStatus(QStringLiteral("Not connected"));
    } else {
        setStatus(QStringLiteral("Connected"));
    }
}

void GameController::stopPlacementTimer() {
    if (placement_timer_ && placement_timer_->isActive()) {
        placement_timer_->stop();
    }
}

void GameController::updatePlacementRemaining() {
    if (!placement_end_.isValid()) {
        return;
    }
    const qint64 left = QDateTime::currentDateTime().secsTo(placement_end_);
    const int n = static_cast<int>(std::max<qint64>(0, left));
    if (n != placement_seconds_remaining_) {
        placement_seconds_remaining_ = n;
        emit placementSecondsRemainingChanged();
    }
}

void GameController::tickPlacementTimer() {
    updatePlacementRemaining();
    if (placement_seconds_remaining_ <= 0 && placement_timer_->isActive()) {
        placement_timer_->stop();
    }
}

void GameController::rebuildCellColors() {
    if (isHider() && isPlacementPhase()) {
        refreshPlacementGridColors();
        return;
    }
    QVariantList out;
    out.reserve(grid_w_ * grid_h_);
    const QString neutral = QStringLiteral("#eeeeee");
    const QString receiverColor = QStringLiteral("#5dade2");
    const QString txRealColor = QStringLiteral("#f1c232");

    const bool showResultBombs = (phase_ == QStringLiteral("Finished")) && !result_tx_positions_.isEmpty();

    for (int y = 0; y < grid_h_; ++y) {
        for (int x = 0; x < grid_w_; ++x) {
            QString color = neutral;
            if (showResultBombs) {
                for (const auto& p : result_tx_positions_) {
                    if (p.first == x && p.second == y) {
                        color = txRealColor;
                        break;
                    }
                }
            }
            if (color == neutral) {
                for (const auto& pos : last_state_data_.receiver_positions) {
                    if (pos.first == x && pos.second == y) {
                        color = receiverColor;
                        break;
                    }
                }
            }
            out.push_back(color);
        }
    }
    cell_colors_ = std::move(out);
    emit cellColorsChanged();
}

} // namespace rssi_game::client
