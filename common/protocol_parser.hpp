#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rssi_game::protocol {

struct InviteData {
    std::string code;
};

struct GameStartData {
    int grid_w{};
    int grid_h{};
    int placement_sec{};
};

struct StateData {
    int turn{};
    int max_turns{};
    std::vector<std::pair<int, int>> receiver_positions;
    std::vector<std::pair<int, int>> tx_estimated;
    /// rssi[rx][tx]
    std::vector<std::vector<double>> rssi;
};

struct PlacementDoneData {
    std::string tx_real_raw;
};

struct GameFinishedData {
    std::string tx_real_raw;
    std::string tx_est_raw;
    double mean_error_cells{};
};

struct ErrorData {
    std::string message;
};

enum class ServerLineKind {
    Unknown,
    Role,
    Invite,
    GameStart,
    State,
    PlacementDone,
    GameFinished,
    Error,
};

struct ParsedServerLine {
    ServerLineKind kind{ServerLineKind::Unknown};
    std::optional<InviteData> invite;
    std::optional<GameStartData> game_start;
    std::optional<StateData> state;
    std::optional<PlacementDoneData> placement_done;
    std::optional<GameFinishedData> game_finished;
    std::optional<ErrorData> error;
    std::string role_raw;
};

/// Parses one line from server (without trailing `\n`). On failure, `out.kind` is Unknown.
void parseServerLine(const std::string& line, ParsedServerLine& out);

/// Parses `x,y;x,y;...` as inside `rxpos=<...>` / `txReal=<...>`.
std::vector<std::pair<int, int>> parseCellPositionList(const std::string& inner);

} // namespace rssi_game::protocol
