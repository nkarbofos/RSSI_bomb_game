// Клиентская часть сетевого взаимодействия (Qt + Boost.Asio или QtNetwork).
// Реализация запросов и протокола будет добавлена позже.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/game_types.hpp"

namespace rssi_game::client {

class NetworkClient {
public:
    NetworkClient();
    void connectToServer(const std::string& host, std::uint16_t port);
    void createLobby();
    void joinLobby(const std::string& invite_code);
    void sendSeekerMove();
    void requestGameState();

    void onGameStateUpdated();
    void onConnectionLost();

private:
    // TODO
};

using NetworkClientPtr = std::shared_ptr<NetworkClient>;

} // namespace rssi_game::client

