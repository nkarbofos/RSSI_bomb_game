#include <QtTest/QtTest>

#include "common/protocol_parser.hpp"

class ProtocolParserTest : public QObject {
    Q_OBJECT

private slots:
    void parseInvite();
    void parseGameStart();
    void parseState();
    void parseGameFinished();
    void parseError();
};

void ProtocolParserTest::parseInvite() {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine("INVITE ABC123", p);
    QCOMPARE(static_cast<int>(p.kind), static_cast<int>(rssi_game::protocol::ServerLineKind::Invite));
    QVERIFY(p.invite.has_value());
    QCOMPARE(QString::fromStdString(p.invite->code), QStringLiteral("ABC123"));
}

void ProtocolParserTest::parseGameStart() {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine("GAME_START 10 10 60", p);
    QCOMPARE(static_cast<int>(p.kind), static_cast<int>(rssi_game::protocol::ServerLineKind::GameStart));
    QVERIFY(p.game_start.has_value());
    QCOMPARE(p.game_start->grid_w, 10);
    QCOMPARE(p.game_start->grid_h, 10);
    QCOMPARE(p.game_start->placement_sec, 60);
}

void ProtocolParserTest::parseState() {
    const std::string line =
        "STATE turn=1/20 rxpos=<5,5;8,3;4,1> txest=<1,2;2,3;7,7> "
        "rssi=<-45.20,-42.76,-39.33|-47.36,-45.82,-44.15|-39.83,-37.37,-37.37>";
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(line, p);
    QCOMPARE(static_cast<int>(p.kind), static_cast<int>(rssi_game::protocol::ServerLineKind::State));
    QVERIFY(p.state.has_value());
    QCOMPARE(p.state->turn, 1);
    QCOMPARE(p.state->max_turns, 20);
    QCOMPARE(p.state->receiver_positions.size(), 3u);
    QCOMPARE(p.state->rssi.size(), 3u);
    QVERIFY(p.state->rssi[0].size() >= 3u);
}

void ProtocolParserTest::parseGameFinished() {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine(
        "GAME_FINISHED txReal=<1,1;2,2;3,3> txEst=<1,2;2,3;7,7> mean_error_cells=1.234", p);
    QCOMPARE(static_cast<int>(p.kind), static_cast<int>(rssi_game::protocol::ServerLineKind::GameFinished));
    QVERIFY(p.game_finished.has_value());
    QCOMPARE(p.game_finished->mean_error_cells, 1.234);
}

void ProtocolParserTest::parseError() {
    rssi_game::protocol::ParsedServerLine p;
    rssi_game::protocol::parseServerLine("ERROR bad things", p);
    QCOMPARE(static_cast<int>(p.kind), static_cast<int>(rssi_game::protocol::ServerLineKind::Error));
    QVERIFY(p.error.has_value());
    QCOMPARE(QString::fromStdString(p.error->message), QStringLiteral("bad things"));
}

QTEST_MAIN(ProtocolParserTest)
#include "client_tests.moc"
