#include "server/game_session.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sstream>

namespace rssi_game::server {

namespace {
double clampToCell(double v, int maxExclusive) {
    if (v < 0.0) return 0.0;
    double maxCell = static_cast<double>(maxExclusive - 1);
    if (v > maxCell) return maxCell;
    return v;
}
} // namespace

GameSession::GameSession() = default;

GamePhase GameSession::phase() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return phase_;
}

GridSize GameSession::gridSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return grid_size_;
}

int GameSession::placementSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return placement_seconds_;
}

int GameSession::turn() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return turn_;
}

bool GameSession::isFinished() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return phase_ == GamePhase::Finished;
}

void GameSession::startPlacement(const GridSize& grid, int placement_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    grid_size_ = grid;
    placement_seconds_ = placement_seconds;
    phase_ = GamePhase::Placement;
    placement_finalized_ = false;

    transmitters_.clear();
    receivers_.clear();
    estimated_transmitters_.clear();
    turn_ = 0;
    finished_at_.reset();
}

void GameSession::setHiderTransmitters(
    const std::vector<CellPosition>& positions) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (phase_ != GamePhase::Placement) {
        throw std::runtime_error("setHiderTransmitters: invalid phase");
    }
    if (positions.size() != 3) {
        throw std::runtime_error("setHiderTransmitters: expected exactly 3 positions");
    }
    if (!positionsAreDistinct(positions)) {
        throw std::runtime_error("setHiderTransmitters: transmitter positions must be distinct");
    }
    for (const auto& p : positions) {
        if (!isCellInsideGrid(p)) {
            throw std::runtime_error("setHiderTransmitters: transmitter position is out of bounds");
        }
    }

    transmitters_ = positions;
    placement_finalized_ = true;
    startSearch();
}

void GameSession::onPlacementTimeout() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (phase_ != GamePhase::Placement) {
        return;
    }
    if (!placement_finalized_) {
        std::vector<CellPosition> all;
        all.reserve(static_cast<size_t>(grid_size_.width * grid_size_.height));
        for (int x = 0; x < grid_size_.width; ++x) {
            for (int y = 0; y < grid_size_.height; ++y) {
                all.push_back(CellPosition{x, y});
            }
        }
        std::mt19937 rng(std::random_device{}());
        std::shuffle(all.begin(), all.end(), rng);

        transmitters_.assign(all.begin(), all.begin() + 3);
        placement_finalized_ = true;
    }
    startSearch();
}

void GameSession::startSearch() {
    if (phase_ != GamePhase::Placement && phase_ != GamePhase::Lobby) {
    }
    if (transmitters_.size() != 3) {
        throw std::runtime_error("startSearch: transmitters_ must be ready");
    }
    phase_ = GamePhase::Search;
    receivers_.clear();
    estimated_transmitters_.clear();
    turn_ = 0;
    initReceiversRandom();
    initEstimatedTransmittersRandom();
}

void GameSession::applySeekerAction(const SeekerAction& action) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (phase_ != GamePhase::Search) {
        throw std::runtime_error("applySeekerAction: invalid phase");
    }
    if (phase_ == GamePhase::Finished) {
        throw std::runtime_error("applySeekerAction: game finished");
    }

    switch (action.type) {
        case SeekerActionType::Done: {
            finishGame();
            return;
        }
        case SeekerActionType::GradientStep: {
            runGradientDescentIteration();
            ++turn_;
            if (turn_ >= max_turns_) {
                finishGame();
            }
            return;
        }
        case SeekerActionType::MoveReceiver: {
            if (action.idx < 0 || static_cast<size_t>(action.idx) >= receivers_.size()) {
                throw std::runtime_error("MoveReceiver: receiver index out of range");
            }
            if (!isCellInsideGrid(action.pos)) {
                throw std::runtime_error("MoveReceiver: target cell out of bounds");
            }

            if (!isCellFreeForReceiver(action.pos, action.idx) &&
                !(receivers_[action.idx].x == action.pos.x &&
                  receivers_[action.idx].y == action.pos.y)) {
                throw std::runtime_error("MoveReceiver: target cell is occupied");
            }

            receivers_[static_cast<size_t>(action.idx)] = action.pos;
            ++turn_;
            if (turn_ >= max_turns_) {
                finishGame();
            }
            return;
        }
        case SeekerActionType::AddReceiver: {
            if (receivers_.size() >= 3) {
                throw std::runtime_error("AddReceiver: receivers already maxed out");
            }
            if (!isCellInsideGrid(action.pos)) {
                throw std::runtime_error("AddReceiver: target cell out of bounds");
            }
            if (!isCellFreeForReceiver(action.pos, -1)) {
                throw std::runtime_error("AddReceiver: target cell is occupied");
            }

            receivers_.push_back(action.pos);
            ++turn_;
            if (turn_ >= max_turns_) {
                finishGame();
            }
            return;
        }
        case SeekerActionType::RemoveReceiver: {
            if (action.idx < 0 || static_cast<size_t>(action.idx) >= receivers_.size()) {
                throw std::runtime_error("RemoveReceiver: receiver index out of range");
            }
            receivers_.erase(receivers_.begin() + action.idx);
            ++turn_;
            if (turn_ >= max_turns_) {
                finishGame();
            }
            return;
        }
    }
    throw std::runtime_error("applySeekerAction: unknown action");
}

std::vector<CellPosition> GameSession::getRealTransmitters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transmitters_;
}

std::vector<CellPosition> GameSession::getReceiverPositions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return receivers_;
}

std::vector<CellPosition> GameSession::getEstimatedTransmitters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return estimated_transmitters_;
}

std::vector<RssiSample> GameSession::getCurrentRssiSamples() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RssiSample> out;
    if (transmitters_.size() != 3) {
        return out;
    }
    out.reserve(receivers_.size() * 3);
    for (const auto& receiver : receivers_) {
        for (const auto& transmitter : transmitters_) {
            out.push_back(
                RssiSample{receiver, transmitter, predictRssi(receiver, transmitter)});
        }
    }
    return out;
}

LocalizationResult GameSession::getLocalizationResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transmitters_.size() != 3 || estimated_transmitters_.size() != 3) {
        return LocalizationResult{};
    }

    double sum = 0.0;
    for (int i = 0; i < 3; ++i) {
        sum += euclideanCellDistance(transmitters_[i], estimated_transmitters_[i]);
    }

    LocalizationResult res;
    res.estimated_transmitters = estimated_transmitters_;
    res.mean_error_cells = sum / 3.0;
    return res;
}

bool GameSession::isCellInsideGrid(CellPosition p) const {
    return p.x >= 0 && p.y >= 0 && p.x < grid_size_.width && p.y < grid_size_.height;
}

bool GameSession::isCellFreeForReceiver(CellPosition p, int ignore_receiver_idx) const {
    if (!isCellInsideGrid(p)) return false;
    for (const auto& tx : transmitters_) {
        if (tx.x == p.x && tx.y == p.y) return false;
    }
    for (size_t i = 0; i < receivers_.size(); ++i) {
        if (static_cast<int>(i) == ignore_receiver_idx) continue;
        if (receivers_[i].x == p.x && receivers_[i].y == p.y) return false;
    }
    return true;
}

bool GameSession::positionsAreDistinct(const std::vector<CellPosition>& positions) {
    if (positions.size() < 2) return true;
    for (size_t i = 0; i < positions.size(); ++i) {
        for (size_t j = i + 1; j < positions.size(); ++j) {
            if (positions[i].x == positions[j].x && positions[i].y == positions[j].y) return false;
        }
    }
    return true;
}

double GameSession::euclideanCellDistance(CellPosition a, CellPosition b) {
    double dx = static_cast<double>(a.x - b.x);
    double dy = static_cast<double>(a.y - b.y);
    return std::sqrt(dx * dx + dy * dy);
}

double GameSession::predictRssi(const CellPosition& receiver,
                                 const CellPosition& transmitter) const {
    double dx = static_cast<double>(receiver.x - transmitter.x);
    double dy = static_cast<double>(receiver.y - transmitter.y);
    double dist = std::sqrt(dx * dx + dy * dy);
    return tx_power_dbm_ - 10.0 * path_loss_n_ * std::log10(dist + eps_);
}

void GameSession::initReceiversRandom() {
    if (grid_size_.width <= 0 || grid_size_.height <= 0) {
        throw std::runtime_error("initReceiversRandom: invalid grid size");
    }
    std::vector<CellPosition> free;
    free.reserve(static_cast<size_t>(grid_size_.width * grid_size_.height));
    for (int x = 0; x < grid_size_.width; ++x) {
        for (int y = 0; y < grid_size_.height; ++y) {
            CellPosition p{x, y};
            bool occupied = false;
            for (const auto& tx : transmitters_) {
                if (tx.x == p.x && tx.y == p.y) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) free.push_back(p);
        }
    }
    if (free.size() < 3) {
        throw std::runtime_error("initReceiversRandom: not enough free cells");
    }

    std::mt19937 rng(std::random_device{}());
    std::shuffle(free.begin(), free.end(), rng);
    receivers_.assign(free.begin(), free.begin() + 3);
}

void GameSession::initEstimatedTransmittersRandom() {
    std::vector<CellPosition> available;
    available.reserve(static_cast<size_t>(grid_size_.width * grid_size_.height));
    for (int x = 0; x < grid_size_.width; ++x) {
        for (int y = 0; y < grid_size_.height; ++y) {
            available.push_back(CellPosition{x, y});
        }
    }
    if (available.size() < 3) {
        estimated_transmitters_ = transmitters_;
        return;
    }

    std::mt19937 rng(std::random_device{}());
    std::shuffle(available.begin(), available.end(), rng);
    estimated_transmitters_.clear();
    for (const auto& p : available) {
        bool distinct = true;
        for (const auto& existing : estimated_transmitters_) {
            if (existing.x == p.x && existing.y == p.y) {
                distinct = false;
                break;
            }
        }
        if (!distinct) continue;
        estimated_transmitters_.push_back(p);
        if (estimated_transmitters_.size() == 3) break;
    }
    while (estimated_transmitters_.size() < 3) {
        estimated_transmitters_.push_back(available.front());
    }
}

void GameSession::runGradientDescentIteration() {
    if (estimated_transmitters_.size() != 3 || transmitters_.size() != 3) {
        return;
    }
    if (receivers_.empty()) {
        return;
    }

    const double ln10 = std::log(10.0);
    const double A = 10.0 * path_loss_n_;

    for (int txIdx = 0; txIdx < 3; ++txIdx) {
        double x = static_cast<double>(estimated_transmitters_[static_cast<size_t>(txIdx)].x);
        double y = static_cast<double>(estimated_transmitters_[static_cast<size_t>(txIdx)].y);

        double grad_x = 0.0;
        double grad_y = 0.0;

        for (const auto& receiver : receivers_) {
            const CellPosition real_tx = transmitters_[static_cast<size_t>(txIdx)];
            double measured = predictRssi(receiver, real_tx);

            double dx = x - static_cast<double>(receiver.x);
            double dy = y - static_cast<double>(receiver.y);
            double dist = std::sqrt(dx * dx + dy * dy);
            double u = dist + eps_;

            double pred = tx_power_dbm_ - A * (std::log10(u));
            double diff = pred - measured;

            if (dist < 1e-9) {
                continue;
            }

            double dpred_dx = -(A / (u * ln10)) * (dx / dist);
            double dpred_dy = -(A / (u * ln10)) * (dy / dist);

            grad_x += diff * dpred_dx;
            grad_y += diff * dpred_dy;
        }

        double x_new = x - learning_rate_ * grad_x;
        double y_new = y - learning_rate_ * grad_y;

        x_new = clampToCell(x_new, grid_size_.width);
        y_new = clampToCell(y_new, grid_size_.height);

        int xi = static_cast<int>(std::llround(x_new));
        int yi = static_cast<int>(std::llround(y_new));
        xi = std::clamp(xi, 0, grid_size_.width - 1);
        yi = std::clamp(yi, 0, grid_size_.height - 1);

        estimated_transmitters_[static_cast<size_t>(txIdx)] = CellPosition{xi, yi};
    }
}

void GameSession::finishGame() {
    phase_ = GamePhase::Finished;
    finished_at_ = std::chrono::steady_clock::now();
}

} // namespace rssi_game::server

