#include "server/game_manager.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>

namespace rssi_game::server {

GameManager::GameManager() = default;

std::string GameManager::createLobby() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (sessions_.size() >= max_simultaneous_games_) {
        throw std::runtime_error("createLobby: too many simultaneous games");
    }

    for (int attempt = 0; attempt < 10; ++attempt) {
        std::string code = generateInviteCode();
        if (sessions_.find(code) == sessions_.end()) {
            sessions_[code] = std::make_shared<GameSession>();
            return code;
        }
    }
    throw std::runtime_error("createLobby: failed to generate unique invite code");
}

GameSessionPtr GameManager::joinLobby(const std::string& invite_code) {
    return findSession(invite_code);
}

GameSessionPtr GameManager::findSession(const std::string& invite_code) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = sessions_.find(invite_code);
    if (it == sessions_.end()) {
        return nullptr;
    }
    return it->second;
}

void GameManager::maintain() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second && it->second->isFinished()) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string GameManager::generateInviteCode() const {
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    constexpr size_t code_len = 6;

    std::mt19937 rng(
        static_cast<uint32_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    constexpr size_t alphabet_len = sizeof(alphabet) - 1;
    std::uniform_int_distribution<size_t> dist(0, alphabet_len - 1);

    std::string code;
    code.resize(code_len);
    for (size_t i = 0; i < code_len; ++i) {
        code[i] = alphabet[dist(rng)];
    }
    return code;
}

} // namespace rssi_game::server

