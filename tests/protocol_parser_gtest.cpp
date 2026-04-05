#include <gtest/gtest.h>

#include "common/protocol_parser.hpp"

TEST(ProtocolParserGTest, Invite) {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine("INVITE ABC123", p);
    EXPECT_EQ(p.kind, rssi_game::protocol::ServerLineKind::Invite);
    ASSERT_TRUE(p.invite.has_value());
    EXPECT_EQ(p.invite->code, "ABC123");
}

TEST(ProtocolParserGTest, GameStart) {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine("GAME_START 10 10 30", p);
    EXPECT_EQ(p.kind, rssi_game::protocol::ServerLineKind::GameStart);
    ASSERT_TRUE(p.game_start.has_value());
    EXPECT_EQ(p.game_start->grid_w, 10);
    EXPECT_EQ(p.game_start->grid_h, 10);
    EXPECT_EQ(p.game_start->placement_sec, 30);
}

TEST(ProtocolParserGTest, State) {
    const std::string line =
        "STATE turn=1/20 rxpos=<5,5;8,3;4,1> txest=<1,2;2,3;7,7> "
        "rssi=<-45.20,-42.76,-39.33|-47.36,-45.82,-44.15|-39.83,-37.37,-37.37>";
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(line, p);
    EXPECT_EQ(p.kind, rssi_game::protocol::ServerLineKind::State);
    ASSERT_TRUE(p.state.has_value());
    EXPECT_EQ(p.state->turn, 1);
    EXPECT_EQ(p.state->max_turns, 20);
    EXPECT_EQ(p.state->receiver_positions.size(), 3u);
    EXPECT_EQ(p.state->rssi.size(), 3u);
    ASSERT_GE(p.state->rssi[0].size(), 3u);
}

TEST(ProtocolParserGTest, GameFinished) {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(
        "GAME_FINISHED txReal=<1,1;2,2;3,3> txEst=<1,2;2,3;7,7> mean_error_cells=1.234", p);
    EXPECT_EQ(p.kind, rssi_game::protocol::ServerLineKind::GameFinished);
    ASSERT_TRUE(p.game_finished.has_value());
    EXPECT_DOUBLE_EQ(p.game_finished->mean_error_cells, 1.234);
}

TEST(ProtocolParserGTest, Error) {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine("ERROR bad things", p);
    EXPECT_EQ(p.kind, rssi_game::protocol::ServerLineKind::Error);
    ASSERT_TRUE(p.error.has_value());
    EXPECT_EQ(p.error->message, "bad things");
}
