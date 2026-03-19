#pragma once

#include <memory>
#include <cstdint>

#include "server/game_manager.hpp"

namespace boost::asio {
class io_context;
} // namespace boost::asio

namespace rssi_game::server {

class NetworkServer {
public:
    NetworkServer(boost::asio::io_context& io, GameManagerPtr game_manager,
                   std::uint16_t port);
    ~NetworkServer();
    void startAccept();
    void stop();

private:
public:
    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
};

using NetworkServerPtr = std::shared_ptr<NetworkServer>;

} // namespace rssi_game::server

