#include "server/network_server.hpp"

#include "server/game_session.hpp"

#include <boost/asio.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cctype>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace rssi_game::server {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

static std::string trimRightCR(std::string s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

static std::string toUpperAscii(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

static bool parseInt(const std::string& token, int& out) {
    try {
        size_t pos = 0;
        int v = std::stoi(token, &pos);
        if (pos != token.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

static std::string formatCellPositionList(const std::vector<CellPosition>& positions) {
    std::ostringstream os;
    for (size_t i = 0; i < positions.size(); ++i) {
        if (i != 0) os << ";";
        os << positions[i].x << "," << positions[i].y;
    }
    return os.str();
}

class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    enum class Role { Unassigned, Seeker, Hider };

    ClientConnection(tcp::socket socket, class NetworkServer::Impl& server_impl)
        : socket_(std::move(socket)),
          strand_(socket_.get_executor()),
          server_(server_impl) {}

    void start();

    void sendLine(const std::string& line);

    Role role() const { return role_; }
    const std::string& inviteCode() const { return invite_code_; }
    GameSessionPtr session() const { return session_; }

    void setAssigned(GameSessionPtr session, Role role, std::string invite_code) {
        session_ = std::move(session);
        role_ = role;
        invite_code_ = std::move(invite_code);
    }

private:
    void doRead();
    void doWrite();
    void onRead(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void handleCommand(const std::string& line);

    void sendError(const std::string& message);

    void sendGameStartIfReady();
    void sendPlacementDone();
    void sendStateToSeeker();
    void sendGameFinished();

    std::string formatStateMessage() const;
    std::string formatGameFinishedMessage() const;
    std::string formatPlacementDoneMessage() const;

private:
    tcp::socket socket_;
    asio::strand<asio::any_io_executor> strand_;
    class NetworkServer::Impl& server_;

    asio::streambuf read_buffer_;
    std::deque<std::string> write_queue_;

    Role role_{Role::Unassigned};
    GameSessionPtr session_{nullptr};
    std::string invite_code_;
};

struct NetworkServer::Impl {
    explicit Impl(asio::io_context& io, GameManagerPtr game_manager, std::uint16_t port)
        : io_(io),
          acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          game_manager_(std::move(game_manager)),
          strand_(asio::make_strand(io)) {}

    void startAccept() { doAccept(); }

    void stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
        for (auto& [_, timer] : placement_timers_) {
            if (timer) {
                boost::system::error_code ignored_ec;
                timer->cancel(ignored_ec);
            }
        }
    }

    void doAccept() {
        acceptor_.async_accept(asio::bind_executor(
            strand_,
            [this](const boost::system::error_code& ec, tcp::socket socket) {
                if (ec) {
                    std::cerr << "[server] accept error: " << ec.message() << "\n";
                    doAccept();
                    return;
                }

                auto connection = std::make_shared<ClientConnection>(std::move(socket), *this);
                connection->start();
                doAccept();
            }));
    }

    struct PlayerConnections {
        std::weak_ptr<ClientConnection> seeker;
        std::weak_ptr<ClientConnection> hider;
    };

    void registerPlayer(const std::string& code, std::weak_ptr<ClientConnection> seekerOrHider,
                         ClientConnection::Role role) {
        auto& entry = players_[code];
        if (role == ClientConnection::Role::Seeker) entry.seeker = std::move(seekerOrHider);
        if (role == ClientConnection::Role::Hider) entry.hider = std::move(seekerOrHider);
    }

    std::shared_ptr<ClientConnection> getSeeker(const std::string& code) {
        auto it = players_.find(code);
        if (it == players_.end()) return nullptr;
        return it->second.seeker.lock();
    }

    std::shared_ptr<ClientConnection> getHider(const std::string& code) {
        auto it = players_.find(code);
        if (it == players_.end()) return nullptr;
        return it->second.hider.lock();
    }

    void cancelPlacementTimer(const std::string& code) {
        auto it = placement_timers_.find(code);
        if (it == placement_timers_.end()) return;
        if (it->second) {
            boost::system::error_code ec;
            it->second->cancel(ec);
        }
        placement_timers_.erase(it);
    }

    void startPlacementTimer(const std::string& code, GameSessionPtr session, int placementSeconds) {
        cancelPlacementTimer(code);

        auto timer = std::make_shared<asio::steady_timer>(io_, std::chrono::seconds(placementSeconds));
        placement_timers_[code] = timer;

        timer->async_wait(asio::bind_executor(
            strand_,
            [this, code, session, placementSeconds](const boost::system::error_code& ec) {
                if (ec == asio::error::operation_aborted) return;

                if (!session) return;
                try {
                    if (session->phase() != GamePhase::Placement) {
                        return;
                    }
                    session->onPlacementTimeout();
                    auto msg = formatPlacementDoneMessage(session);

                    auto seeker = getSeeker(code);
                    auto hider = getHider(code);
                    if (seeker) seeker->sendLine(msg);
                    if (hider) hider->sendLine(msg);
                } catch (const std::exception& ex) {
                    std::cerr << "[server] placement timeout failure: " << ex.what() << "\n";
                }

                cancelPlacementTimer(code);
                (void)placementSeconds;
            }));
    }

    std::string formatPlacementDoneMessage(const GameSessionPtr& session) const {
        auto txReal = session->getRealTransmitters();
        std::ostringstream os;
        os << "PLACEMENT_DONE txReal=<" << formatCellPositionList(txReal) << ">\n";
        return os.str();
    }

    asio::io_context& io_;
    tcp::acceptor acceptor_;
    GameManagerPtr game_manager_;
    asio::strand<asio::any_io_executor> strand_;

    std::unordered_map<std::string, PlayerConnections> players_;
    std::unordered_map<std::string, std::shared_ptr<asio::steady_timer>> placement_timers_;
};

NetworkServer::NetworkServer(asio::io_context& io, GameManagerPtr game_manager,
                               std::uint16_t port)
    : impl_(std::make_unique<Impl>(io, std::move(game_manager), port)) {}

NetworkServer::~NetworkServer() = default;

void NetworkServer::startAccept() { impl_->startAccept(); }

void NetworkServer::stop() { impl_->stop(); }

void ClientConnection::start() {
    doRead();
}

void ClientConnection::doRead() {
    auto self = shared_from_this();
    asio::async_read_until(
        socket_, read_buffer_, '\n',
        asio::bind_executor(
            strand_,
            [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                (void)bytes_transferred;
                self->onRead(ec, 0);
            }));
}

void ClientConnection::onRead(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        std::cerr << "[server] client read error: " << ec.message() << "\n";
        boost::system::error_code ignored_ec;
        socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
        return;
    }

    std::istream is(&read_buffer_);
    std::string line;
    std::getline(is, line);
    line = trimRightCR(line);

    if (!line.empty()) {
        handleCommand(line);
    }

    doRead();
}

void ClientConnection::sendLine(const std::string& line) {
    auto msg = line;
    if (msg.empty() || msg.back() != '\n') msg.push_back('\n');

    auto self = shared_from_this();
    asio::post(
        strand_,
        [this, self, msg = std::move(msg)]() mutable {
            const bool need_write = write_queue_.empty();
            write_queue_.push_back(std::move(msg));
            if (need_write) {
                doWrite();
            }
        });
}

void ClientConnection::doWrite() {
    if (write_queue_.empty()) return;
    auto self = shared_from_this();
    asio::async_write(
        socket_, asio::buffer(write_queue_.front()),
        asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes*/) {
                if (ec) {
                    std::cerr << "[server] client write error: " << ec.message() << "\n";
                    boost::system::error_code ignored_ec;
                    socket_.close(ignored_ec);
                    write_queue_.clear();
                    return;
                }
                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    doWrite();
                }
            }));
}

void ClientConnection::sendError(const std::string& message) {
    sendLine("ERROR " + message);
}

void ClientConnection::handleCommand(const std::string& line) {
    try {
        std::istringstream is(line);
        std::string cmd;
        if (!(is >> cmd)) return;
        cmd = toUpperAscii(cmd);

        if (cmd == "CREATE_LOBBY") {
            if (role_ != Role::Unassigned) {
                sendError("CREATE_LOBBY not allowed after role assignment");
                return;
            }
            std::string code = server_.game_manager_->createLobby();
            auto session = server_.game_manager_->findSession(code);
            if (!session) {
                sendError("Failed to create lobby");
                return;
            }
            setAssigned(session, Role::Seeker, code);
            server_.registerPlayer(code, weak_from_this(), Role::Seeker);
            std::cout << "[server] seeker created lobby: " << code << "\n";
            sendLine("ROLE Seeker");
            sendLine("INVITE " + code);
            sendGameStartIfReady();
            return;
        }

        if (cmd == "JOIN_LOBBY") {
            if (role_ != Role::Unassigned) {
                sendError("JOIN_LOBBY not allowed after role assignment");
                return;
            }
            std::string code;
            if (!(is >> code)) {
                sendError("JOIN_LOBBY requires invite code");
                return;
            }
            code = trimRightCR(code);
            auto session = server_.game_manager_->joinLobby(code);
            if (!session) {
                sendError("Invite code not found");
                return;
            }
            setAssigned(session, Role::Hider, code);
            server_.registerPlayer(code, weak_from_this(), Role::Hider);
            std::cout << "[server] hider joined lobby: " << code << "\n";
            sendLine("ROLE Hider");

            GamePhase p = session->phase();
            if (p == GamePhase::Lobby || p == GamePhase::Finished) {
                GridSize grid{10, 10};
                constexpr int placementSeconds = 30;
                session->startPlacement(grid, placementSeconds);
            }

            sendGameStartIfReady();
            server_.startPlacementTimer(code, session, 30);
            return;
        }

        if (cmd == "PLACE_TRANSMITTERS") {
            if (role_ != Role::Hider) {
                sendError("PLACE_TRANSMITTERS allowed only for hider");
                return;
            }
            if (!session_) {
                sendError("No session assigned");
                return;
            }
            if (session_->phase() != GamePhase::Placement) {
                sendError("PLACE_TRANSMITTERS only in placement phase");
                return;
            }

            int x1, y1, x2, y2, x3, y3;
            if (!(is >> x1 >> y1 >> x2 >> y2 >> x3 >> y3)) {
                sendError("PLACE_TRANSMITTERS requires 6 ints");
                return;
            }

            std::vector<CellPosition> positions{{x1, y1}, {x2, y2}, {x3, y3}};
            session_->setHiderTransmitters(positions);
            server_.cancelPlacementTimer(invite_code_);
            sendPlacementDone();
            return;
        }

        if (role_ != Role::Seeker) {
            sendError("Only seeker can send game actions");
            return;
        }
        if (!session_) {
            sendError("No session assigned");
            return;
        }
        if (cmd == "DONE") {
            session_->applySeekerAction(SeekerAction{SeekerActionType::Done});
            sendGameFinished();
            return;
        }

        if (session_->phase() != GamePhase::Search) {
            sendError("Game actions allowed only in search phase");
            return;
        }

        if (cmd == "GRADIENT_STEP") {
            session_->applySeekerAction(SeekerAction{SeekerActionType::GradientStep});
            if (session_->isFinished()) {
                sendGameFinished();
            } else {
                sendStateToSeeker();
            }
            return;
        }

        if (cmd == "MOVE_RECEIVER") {
            int idx, x, y;
            if (!(is >> idx >> x >> y)) {
                sendError("MOVE_RECEIVER requires: idx x y");
                return;
            }
            SeekerAction action;
            action.type = SeekerActionType::MoveReceiver;
            action.idx = idx;
            action.pos = CellPosition{x, y};
            session_->applySeekerAction(action);
            if (session_->isFinished()) sendGameFinished();
            else sendStateToSeeker();
            return;
        }

        if (cmd == "ADD_RECEIVER") {
            int x, y;
            if (!(is >> x >> y)) {
                sendError("ADD_RECEIVER requires: x y");
                return;
            }
            SeekerAction action;
            action.type = SeekerActionType::AddReceiver;
            action.pos = CellPosition{x, y};
            session_->applySeekerAction(action);
            if (session_->isFinished()) sendGameFinished();
            else sendStateToSeeker();
            return;
        }

        if (cmd == "REMOVE_RECEIVER") {
            int idx;
            if (!(is >> idx)) {
                sendError("REMOVE_RECEIVER requires: idx");
                return;
            }
            SeekerAction action;
            action.type = SeekerActionType::RemoveReceiver;
            action.idx = idx;
            session_->applySeekerAction(action);
            if (session_->isFinished()) sendGameFinished();
            else sendStateToSeeker();
            return;
        }

        sendError("Unknown command: " + cmd);
    } catch (const std::exception& ex) {
        sendError(ex.what());
    }
}

void ClientConnection::sendGameStartIfReady() {
    if (invite_code_.empty()) return;

    auto seeker = server_.getSeeker(invite_code_);
    auto hider = server_.getHider(invite_code_);
    if (!seeker || !hider) return;
    if (!session_) {
        session_ = seeker->session();
    }
    if (!session_) return;

    auto grid = session_->gridSize();
    constexpr int placementSeconds = 30;
    std::ostringstream os;
    os << "GAME_START " << grid.width << " " << grid.height << " " << placementSeconds;
    os << "\n";

    seeker->sendLine(os.str());
    hider->sendLine(os.str());

    auto stateMsg = seeker->formatStateMessage();
    seeker->sendLine(stateMsg);
}

void ClientConnection::sendPlacementDone() {
    if (!session_) return;
    auto msg = server_.formatPlacementDoneMessage(session_);
    auto seeker = server_.getSeeker(invite_code_);
    auto hider = server_.getHider(invite_code_);
    if (seeker) seeker->sendLine(msg);
    if (hider) hider->sendLine(msg);
}

std::string ClientConnection::formatStateMessage() const {
    if (!session_) return "ERROR no session\n";

    constexpr int maxTurns = 20;
    auto txest = session_->getEstimatedTransmitters();
    auto rxpos = session_->getReceiverPositions();
    auto rssiSamples = session_->getCurrentRssiSamples();

    std::ostringstream os;
    os << "STATE turn=" << session_->turn() << "/" << maxTurns;
    os << " rxpos=<" << formatCellPositionList(rxpos) << ">";
    os << " txest=<" << formatCellPositionList(txest) << ">";

    os << " rssi=<";
    size_t receiverCount = rxpos.size();
    for (size_t i = 0; i < receiverCount; ++i) {
        if (i != 0) os << "|";
        for (size_t tx = 0; tx < 3; ++tx) {
            if (tx != 0) os << ",";
            auto sample = rssiSamples[i * 3 + tx];
            os << std::fixed << std::setprecision(2) << sample.rssi;
        }
    }
    os << ">\n";
    return os.str();
}

void ClientConnection::sendStateToSeeker() {
    auto msg = formatStateMessage();
    auto seeker = server_.getSeeker(invite_code_);
    if (seeker) seeker->sendLine(msg);
}

std::string ClientConnection::formatPlacementDoneMessage() const {
    if (!session_) return "";
    return server_.formatPlacementDoneMessage(session_);
}

std::string ClientConnection::formatGameFinishedMessage() const {
    if (!session_) return "ERROR no session\n";
    auto txReal = session_->getRealTransmitters();
    auto txEst = session_->getEstimatedTransmitters();
    auto loc = session_->getLocalizationResult();

    std::ostringstream os;
    os << "GAME_FINISHED txReal=<" << formatCellPositionList(txReal) << ">";
    os << " txEst=<" << formatCellPositionList(txEst) << ">";
    os << " mean_error_cells=" << std::fixed << std::setprecision(3) << loc.mean_error_cells;
    os << "\n";
    return os.str();
}

void ClientConnection::sendGameFinished() {
    auto msg = formatGameFinishedMessage();
    auto seeker = server_.getSeeker(invite_code_);
    auto hider = server_.getHider(invite_code_);
    if (seeker) seeker->sendLine(msg);
    if (hider) hider->sendLine(msg);

    server_.game_manager_->maintain();
}

} // namespace rssi_game::server

