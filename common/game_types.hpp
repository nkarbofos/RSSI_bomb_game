#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rssi_game {

enum class PlayerRole {
    Seeker,
    Hider
};

enum class GamePhase {
    Lobby,
    Placement,
    Search,
    Finished
};

struct GridSize {
    int width{};
    int height{};
};

struct CellPosition {
    int x{};
    int y{};
};

struct RssiSample {
    CellPosition receiver;
    CellPosition transmitter;
    double rssi{};
};

struct LocalizationResult {
    std::vector<CellPosition> estimated_transmitters;
    double mean_error_cells{};
};

} // namespace rssi_game

