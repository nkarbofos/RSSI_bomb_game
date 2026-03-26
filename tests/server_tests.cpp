#include <boost/asio.hpp>

#include <cassert>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdexcept>
#include <string>
#include <thread>

#include "server/game_session.hpp"
#include "server/game_manager.hpp"
#include "server/network_server.hpp"

namespace {
using boost::asio::ip::tcp;
using namespace rssi_game::server;
using namespace rssi_game;

std::string recvLine(boost::asio::ip::tcp::socket& socket,
                     boost::asio::streambuf& buf,
                     std::chrono::milliseconds timeout) {
    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    boost::system::error_code ec;
    std::size_t n = boost::asio::read_until(socket, buf, '\n', ec);
    (void)n;
    if (ec) {
        throw std::runtime_error(std::string("recvLine failed: ") + ec.message());
    }
    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

void sendLine(boost::asio::ip::tcp::socket& socket, const std::string& line) {
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
    boost::asio::write(socket, boost::asio::buffer(msg));
}

void expectThrows(const std::string& what, const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception& ex) {
        (void)ex;
        return;
    }
    throw std::runtime_error("Expected exception: " + what);
}

void unit_tests_game_session() {
    GameSession session;
    GridSize grid{10, 10};

    expectThrows("GradientStep in Placement", [&] {
        session.startPlacement(grid, 30);
        session.applySeekerAction(SeekerAction{SeekerActionType::GradientStep});
    });

    session.startPlacement(grid, 30);
    session.setHiderTransmitters({CellPosition{1, 1}, CellPosition{2, 2}, CellPosition{3, 3}});

    expectThrows("AddReceiver when receivers already 3", [&] {
        session.applySeekerAction(SeekerAction{SeekerActionType::AddReceiver, -1, CellPosition{4, 4}});
    });

    auto receivers = session.getReceiverPositions();
    assert(receivers.size() == 3);
    expectThrows("MoveReceiver to occupied cell", [&] {
        SeekerAction act;
        act.type = SeekerActionType::MoveReceiver;
        act.idx = 0;
        act.pos = receivers[1];
        session.applySeekerAction(act);
    });

    session.applySeekerAction(SeekerAction{SeekerActionType::Done});
    assert(session.isFinished());

    expectThrows("Command after DONE", [&] {
        session.applySeekerAction(SeekerAction{SeekerActionType::GradientStep});
    });
}

void integration_tests_protocol() {
    const unsigned short port = static_cast<unsigned short>(50000 + (getpid() % 10000));

    boost::asio::io_context serverIo;
    auto gm = std::make_shared<rssi_game::server::GameManager>();
    rssi_game::server::NetworkServer server(serverIo, gm, port);

    server.startAccept();
    std::thread t([&] { serverIo.run(); });

    try {
        auto endpoint =
            tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
        boost::asio::io_context cIo1;
        boost::asio::io_context cIo2;

        auto connectWithRetry = [&](boost::asio::io_context& io) -> tcp::socket {
            for (int attempt = 0; attempt < 50; ++attempt) {
                boost::system::error_code ec;
                tcp::socket s(io);
                s.connect(endpoint, ec);
                if (!ec) return s;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            throw std::runtime_error("Failed to connect client to test server");
        };

        tcp::socket seeker = connectWithRetry(cIo1);
        tcp::socket hider = connectWithRetry(cIo2);

        boost::asio::streambuf seekerBuf;
        boost::asio::streambuf hiderBuf;

        sendLine(seeker, "CREATE_LOBBY");
        (void)recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        std::string inviteLine = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        if (inviteLine.rfind("INVITE ", 0) != 0) {
            throw std::runtime_error("Unexpected invite line: " + inviteLine);
        }
        std::string code = inviteLine.substr(std::string("INVITE ").size());

        sendLine(hider, "JOIN_LOBBY " + code);
        (void)recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));

        std::string startSeeker = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        std::string startHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
        if (startSeeker.rfind("GAME_START", 0) != 0) {
            throw std::runtime_error("Unexpected startSeeker: " + startSeeker);
        }
        if (startHider.rfind("GAME_START", 0) != 0) {
            throw std::runtime_error("Unexpected startHider: " + startHider);
        }

        std::string state0 = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        if (state0.rfind("STATE", 0) != 0) {
            throw std::runtime_error("Expected STATE after GAME_START, got: " + state0);
        }

        sendLine(hider, "PLACE_TRANSMITTERS 1 1 2 2 3 3");
        std::string placementSeeker = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        std::string placementHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
        if (placementSeeker.rfind("PLACEMENT_DONE", 0) != 0) {
            throw std::runtime_error("Unexpected placementSeeker: " + placementSeeker);
        }
        if (placementHider.rfind("PLACEMENT_DONE", 0) != 0) {
            throw std::runtime_error("Unexpected placementHider: " + placementHider);
        }

        sendLine(seeker, "DONE");
        std::string finishedSeeker = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        std::string finishedHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
        if (finishedSeeker.rfind("GAME_FINISHED", 0) != 0) {
            throw std::runtime_error("Unexpected finishedSeeker: " + finishedSeeker);
        }
        if (finishedHider.rfind("GAME_FINISHED", 0) != 0) {
            throw std::runtime_error("Unexpected finishedHider: " + finishedHider);
        }

        sendLine(seeker, "GRADIENT_STEP");
        std::string errAfterDone = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
        if (errAfterDone.rfind("ERROR", 0) != 0) {
            throw std::runtime_error("Expected ERROR after DONE, got: " + errAfterDone);
        }

        seeker.close();
        hider.close();
    } catch (...) {
        server.stop();
        serverIo.stop();
        t.join();
        throw;
    }

    server.stop();
    serverIo.stop();
    t.join();
}

}

int main() {
    try {
        unit_tests_game_session();
        integration_tests_protocol();
        std::cout << "[tests] OK\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[tests] FAILED: " << ex.what() << "\n";
        return 1;
    }
}

