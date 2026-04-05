#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <chrono>

#include "common/game_types.hpp"

namespace rssi_game::server {

enum class SeekerActionType {
    MoveReceiver,
    AddReceiver,
    RemoveReceiver,
    GradientStep,
    Done
};

struct SeekerAction {
    SeekerActionType type{SeekerActionType::GradientStep};
    int idx{-1};
    CellPosition pos{};
};

class GameSession {
public:
    GameSession();

    GamePhase phase() const;
    GridSize gridSize() const;
    int placementSeconds() const;
    int turn() const;
    bool isFinished() const;

    void startPlacement(const GridSize& grid, int placement_seconds);
    void setHiderTransmitters(const std::vector<CellPosition>& positions);
    void onPlacementTimeout();

    void startSearch();
    void applySeekerAction(const SeekerAction& action);

    std::vector<CellPosition> getRealTransmitters() const;
    std::vector<CellPosition> getReceiverPositions() const;
    std::vector<CellPosition> getEstimatedTransmitters() const;

    std::vector<RssiSample> getCurrentRssiSamples() const;
    LocalizationResult getLocalizationResult() const;

private:
    void finishGame();

    bool isCellInsideGrid(CellPosition p) const;
    bool isCellFreeForReceiver(CellPosition p, int ignore_receiver_idx) const;
    static bool positionsAreDistinct(const std::vector<CellPosition>& positions);
    static double euclideanCellDistance(CellPosition a, CellPosition b);

    double predictRssi(const CellPosition& receiver, const CellPosition& transmitter) const;

    void initReceiversRandom();
    void initEstimatedTransmittersRandom();
    void runGradientDescentIteration();

private:
    mutable std::mutex mutex_;

    GamePhase phase_{GamePhase::Lobby};
    GridSize grid_size_{};
    int placement_seconds_{30};

    bool placement_finalized_{false};
    std::vector<CellPosition> transmitters_;
    std::vector<CellPosition> receivers_;
    std::vector<CellPosition> estimated_transmitters_;

    int turn_{0};
    static constexpr int max_turns_{20};

    std::optional<std::chrono::steady_clock::time_point> finished_at_;

    double tx_power_dbm_{-30.0};
    double path_loss_n_{2.0};
    double eps_{0.1};
    double learning_rate_{0.05};
};

using GameSessionPtr = std::shared_ptr<GameSession>;

} // namespace rssi_game::server

