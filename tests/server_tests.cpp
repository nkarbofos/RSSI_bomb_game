#include <boost/asio.hpp>

#include <functional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "server/game_session.hpp"
#include "tests/test_helpers.hpp"

namespace {
using boost::asio::ip::tcp;
using namespace rssi_game::server;
using namespace rssi_game;
using rssi_game::test::connectWithRetry;
using rssi_game::test::recvLine;
using rssi_game::test::sendLine;
using rssi_game::test::TestServerFixture;

void expectThrows(const std::string& what, const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception& ex) {
        (void)ex;
        (void)what;
        return;
    }
    throw std::runtime_error("Expected exception: " + what);
}

TEST(GameSession, GradientStepInPlacementThrows) {
    GameSession session;
    GridSize grid{10, 10};
    expectThrows("GradientStep in Placement", [&] {
        session.startPlacement(grid, 30);
        session.applySeekerAction(SeekerAction{SeekerActionType::GradientStep});
    });
}

TEST(GameSession, PlacementAndSearchRules) {
    GameSession session;
    GridSize grid{10, 10};

    session.startPlacement(grid, 30);
    session.setHiderTransmitters({CellPosition{1, 1}, CellPosition{2, 2}, CellPosition{3, 3}});

    expectThrows("AddReceiver when receivers already 3", [&] {
        session.applySeekerAction(SeekerAction{SeekerActionType::AddReceiver, -1, CellPosition{4, 4}});
    });

    auto receivers = session.getReceiverPositions();
    ASSERT_EQ(receivers.size(), 3u);
    expectThrows("MoveReceiver to occupied cell", [&] {
        SeekerAction act;
        act.type = SeekerActionType::MoveReceiver;
        act.idx = 0;
        act.pos = receivers[1];
        session.applySeekerAction(act);
    });

    session.applySeekerAction(SeekerAction{SeekerActionType::Done});
    ASSERT_TRUE(session.isFinished());

    expectThrows("Command after DONE", [&] {
        session.applySeekerAction(SeekerAction{SeekerActionType::GradientStep});
    });
}

TEST_F(TestServerFixture, FullProtocolFlow) {
    auto endpoint = tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port());

    boost::asio::io_context cIo1;
    boost::asio::io_context cIo2;

    tcp::socket seeker = connectWithRetry(cIo1, endpoint);
    tcp::socket hider = connectWithRetry(cIo2, endpoint);

    boost::asio::streambuf seekerBuf;
    boost::asio::streambuf hiderBuf;

    sendLine(seeker, "CREATE_LOBBY");
    (void)recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    std::string inviteLine = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    ASSERT_EQ(inviteLine.rfind("INVITE ", 0), 0u) << "Unexpected invite line: " << inviteLine;
    std::string code = inviteLine.substr(std::string("INVITE ").size());

    sendLine(hider, "JOIN_LOBBY " + code);
    (void)recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));

    std::string startSeeker = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    std::string startHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
    ASSERT_EQ(startSeeker.rfind("GAME_START", 0), 0u) << "Unexpected startSeeker: " << startSeeker;
    ASSERT_EQ(startHider.rfind("GAME_START", 0), 0u) << "Unexpected startHider: " << startHider;

    sendLine(hider, "PLACE_TRANSMITTERS 1 1 2 2 3 3");
    std::string placementSeeker = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    std::string stateAfterPlacement = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    std::string placementHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
    ASSERT_EQ(placementSeeker.rfind("PLACEMENT_DONE", 0), 0u)
        << "Unexpected placementSeeker: " << placementSeeker;
    ASSERT_EQ(stateAfterPlacement.rfind("STATE", 0), 0u)
        << "Expected STATE after PLACEMENT_DONE for seeker, got: " << stateAfterPlacement;
    ASSERT_EQ(placementHider.rfind("PLACEMENT_DONE", 0), 0u)
        << "Unexpected placementHider: " << placementHider;
    std::string stateAfterPlacementHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
    ASSERT_EQ(stateAfterPlacementHider.rfind("STATE", 0), 0u)
        << "Expected STATE after PLACEMENT_DONE for hider, got: " << stateAfterPlacementHider;

    sendLine(seeker, "DONE");
    std::string finishedSeeker = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    std::string finishedHider = recvLine(hider, hiderBuf, std::chrono::milliseconds(2000));
    ASSERT_EQ(finishedSeeker.rfind("GAME_FINISHED", 0), 0u)
        << "Unexpected finishedSeeker: " << finishedSeeker;
    ASSERT_EQ(finishedHider.rfind("GAME_FINISHED", 0), 0u)
        << "Unexpected finishedHider: " << finishedHider;

    sendLine(seeker, "GRADIENT_STEP");
    std::string errAfterDone = recvLine(seeker, seekerBuf, std::chrono::milliseconds(2000));
    ASSERT_EQ(errAfterDone.rfind("ERROR", 0), 0u) << "Expected ERROR after DONE, got: " << errAfterDone;

    seeker.close();
    hider.close();
}

} // namespace
