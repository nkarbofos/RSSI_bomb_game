#include <boost/asio.hpp>

#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "tests/test_helpers.hpp"

namespace {
using boost::asio::ip::tcp;
using rssi_game::test::connectWithRetry;
using rssi_game::test::recvLine;
using rssi_game::test::sendLine;
using rssi_game::test::TestServerFixture;
using rssi_game::test::tryRecvLine;

TEST_F(TestServerFixture, UnknownCommand_ReturnsError) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());
    boost::asio::io_context io;
    tcp::socket s = connectWithRetry(io, endpoint);
    boost::asio::streambuf buf;
    sendLine(s, "FOO_BAR");
    std::string line = recvLine(s, buf, std::chrono::milliseconds(2000));
    EXPECT_EQ(line.rfind("ERROR", 0), 0u) << line;
}

TEST_F(TestServerFixture, CreateLobbyWithExtraTokens_StillCreatesLobby) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());
    boost::asio::io_context io;
    tcp::socket s = connectWithRetry(io, endpoint);
    boost::asio::streambuf buf;
    sendLine(s, "CREATE_LOBBY extra_token");
    (void)recvLine(s, buf, std::chrono::milliseconds(2000));
    std::string inviteLine = recvLine(s, buf, std::chrono::milliseconds(2000));
    EXPECT_EQ(inviteLine.rfind("INVITE ", 0), 0u) << inviteLine;
}

TEST_F(TestServerFixture, VeryLongLine_ServerRespondsWithoutHang) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());
    boost::asio::io_context io;
    tcp::socket s = connectWithRetry(io, endpoint);
    boost::asio::streambuf buf;
    std::string longLine(128 * 1024, 'A');
    sendLine(s, longLine);
    std::string line;
    ASSERT_TRUE(tryRecvLine(s, buf, std::chrono::milliseconds(5000), line));
    EXPECT_EQ(line.rfind("ERROR", 0), 0u) << line.substr(0, 80);
}

TEST_F(TestServerFixture, BinaryBytesFollowedByNewline_ErrorOrStable) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());
    boost::asio::io_context io;
    tcp::socket s = connectWithRetry(io, endpoint);
    boost::asio::streambuf buf;
    std::string junk;
    junk.push_back('\0');
    junk.push_back(static_cast<char>(0xff));
    junk.push_back('\n');
    boost::asio::write(s, boost::asio::buffer(junk));
    std::string line;
    ASSERT_TRUE(tryRecvLine(s, buf, std::chrono::milliseconds(2000), line));
    EXPECT_EQ(line.rfind("ERROR", 0), 0u) << line;
}

TEST_F(TestServerFixture, PartialLineThenFullCommand_ParsesAsOneLine) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());
    boost::asio::io_context io;
    tcp::socket s = connectWithRetry(io, endpoint);
    boost::asio::streambuf buf;
    boost::asio::write(s, boost::asio::buffer(std::string("CRE")));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sendLine(s, "ATE_LOBBY");
    (void)recvLine(s, buf, std::chrono::milliseconds(2000));
    std::string inviteLine = recvLine(s, buf, std::chrono::milliseconds(2000));
    EXPECT_EQ(inviteLine.rfind("INVITE ", 0), 0u) << inviteLine;
}

TEST_F(TestServerFixture, SqlLikeString_UnknownCommandError) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());
    boost::asio::io_context io;
    tcp::socket s = connectWithRetry(io, endpoint);
    boost::asio::streambuf buf;
    sendLine(s, "'; DROP TABLE users; --");
    std::string line = recvLine(s, buf, std::chrono::milliseconds(2000));
    EXPECT_EQ(line.rfind("ERROR", 0), 0u) << line;
}

} // namespace
