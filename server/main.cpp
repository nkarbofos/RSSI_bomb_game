#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "server/game_manager.hpp"
#include "server/network_server.hpp"

int main(int argc, char** argv) {
    try {
        std::uint16_t port = 3000;
        if (argc >= 2) {
            port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        }

        boost::asio::io_context io;
        auto game_manager = std::make_shared<rssi_game::server::GameManager>();
        rssi_game::server::NetworkServer server(io, game_manager, port);

        server.startAccept();
        std::cout << "rssi_bomb_server listening on port " << port << std::endl;
        io.run();
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

