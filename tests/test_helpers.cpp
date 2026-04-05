#include "tests/test_helpers.hpp"

#include "server/game_manager.hpp"
#include "server/network_server.hpp"

#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <stdexcept>
#include <thread>

namespace rssi_game::test {

std::string recvLine(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf& buf,
                     std::chrono::milliseconds timeout) {
    std::string out;
    if (!tryRecvLine(socket, buf, timeout, out)) {
        throw std::runtime_error("recvLine failed or timed out");
    }
    return out;
}

bool tryRecvLine(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf& buf,
                 std::chrono::milliseconds timeout, std::string& out) {
    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    boost::system::error_code ec;
    (void)boost::asio::read_until(socket, buf, '\n', ec);
    if (ec) {
        return false;
    }
    std::istream is(&buf);
    std::getline(is, out);
    if (!out.empty() && out.back() == '\r') {
        out.pop_back();
    }
    return true;
}

void sendLine(boost::asio::ip::tcp::socket& socket, const std::string& line) {
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n') {
        msg.push_back('\n');
    }
    boost::asio::write(socket, boost::asio::buffer(msg));
}

boost::asio::ip::tcp::socket connectWithRetry(boost::asio::io_context& io,
                                               const boost::asio::ip::tcp::endpoint& endpoint,
                                               int max_attempts) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        boost::system::error_code ec;
        tcp::socket s(io);
        s.connect(endpoint, ec);
        if (!ec) {
            return s;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    throw std::runtime_error("Failed to connect client to test server");
}

void TestServerFixture::SetUp() {
    port_ = static_cast<std::uint16_t>(50000 + (getpid() % 10000));
    game_manager_ = std::make_shared<rssi_game::server::GameManager>();
    server_ = std::make_unique<rssi_game::server::NetworkServer>(server_io_, game_manager_, port_);
    server_->startAccept();
    server_thread_ = std::thread([this] { server_io_.run(); });
}

void TestServerFixture::TearDown() {
    if (server_) {
        server_->stop();
    }
    server_io_.stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    server_.reset();
}

} // namespace rssi_game::test
