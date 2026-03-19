#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

#include "server/game_session.hpp"

namespace rssi_game::server {

class GameManager {
public:
    GameManager();
    std::string createLobby();
    GameSessionPtr joinLobby(const std::string& invite_code);
    GameSessionPtr findSession(const std::string& invite_code);
    void maintain();

private:
    static constexpr size_t max_simultaneous_games_{100};

    std::unordered_map<std::string, GameSessionPtr> sessions_;
    mutable std::mutex mutex_;
    std::string generateInviteCode() const;
};

using GameManagerPtr = std::shared_ptr<GameManager>;

} // namespace rssi_game::server

