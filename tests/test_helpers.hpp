#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "server/game_manager.hpp"
#include "server/network_server.hpp"

namespace rssi_game::test {

std::string recvLine(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf& buf,
                     std::chrono::milliseconds timeout);

void sendLine(boost::asio::ip::tcp::socket& socket, const std::string& line);

/// Reads one line; returns false on timeout or read error (socket stays open if possible).
bool tryRecvLine(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf& buf,
                 std::chrono::milliseconds timeout, std::string& out);

boost::asio::ip::tcp::socket connectWithRetry(boost::asio::io_context& io,
                                               const boost::asio::ip::tcp::endpoint& endpoint,
                                               int max_attempts = 50);

class TestServerFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::uint16_t port() const noexcept { return port_; }

private:
    boost::asio::io_context server_io_{};
    std::shared_ptr<rssi_game::server::GameManager> game_manager_{};
    std::unique_ptr<rssi_game::server::NetworkServer> server_{};
    std::thread server_thread_{};
    std::uint16_t port_{};
};

} // namespace rssi_game::test
