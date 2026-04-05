#include "client/network_client.hpp"

#include "common/protocol_parser.hpp"

#include <boost/asio.hpp>
#include <boost/asio/post.hpp>

#include <QString>

#include <future>
#include <istream>
#include <sstream>
#include <thread>
#include <utility>

namespace rssi_game::client {
namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

} // namespace

struct NetworkClient::Impl {
    std::shared_ptr<asio::io_context> io;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work;
    std::thread thread;
    std::shared_ptr<tcp::socket> socket;
    asio::streambuf read_buf;
    NetworkClient* self{nullptr};

    explicit Impl(NetworkClient* s) : self(s) {}

    void runIo() { io->run(); }

    void handleLine(const std::string& line) {
        rssi_game::protocol::ParsedServerLine p;
        rssi_game::protocol::parseServerLine(line, p);
        switch (p.kind) {
        case rssi_game::protocol::ServerLineKind::Role:
            emit self->roleReceived(QString::fromStdString(p.role_raw));
            break;
        case rssi_game::protocol::ServerLineKind::Invite:
            if (p.invite) {
                emit self->inviteReceived(QString::fromStdString(p.invite->code));
            }
            break;
        case rssi_game::protocol::ServerLineKind::GameStart:
            if (p.game_start) {
                emit self->gameStarted(p.game_start->grid_w, p.game_start->grid_h, p.game_start->placement_sec);
            }
            break;
        case rssi_game::protocol::ServerLineKind::State:
            emit self->stateLineReceived(QString::fromStdString(line));
            break;
        case rssi_game::protocol::ServerLineKind::PlacementDone:
            emit self->placementDone(QString::fromStdString(line));
            break;
        case rssi_game::protocol::ServerLineKind::GameFinished:
            emit self->gameFinished(QString::fromStdString(line));
            break;
        case rssi_game::protocol::ServerLineKind::Error:
            if (p.error) {
                emit self->errorReceived(QString::fromStdString(p.error->message));
            }
            break;
        default:
            break;
        }
    }

    void startRead() {
        if (!socket || !socket->is_open()) {
            return;
        }
        auto sock = socket;
        auto buf = &read_buf;
        asio::async_read_until(
            *sock, *buf, '\n',
            [this, sock](const boost::system::error_code& ec, std::size_t /*n*/) {
                if (ec) {
                    emit self->connectionChanged(false);
                    return;
                }
                std::istream is(&read_buf);
                std::string line;
                std::getline(is, line);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!line.empty()) {
                    handleLine(line);
                }
                startRead();
            });
    }

    void sendLine(const std::string& line) {
        if (!socket || !socket->is_open()) {
            return;
        }
        std::string msg = line;
        if (msg.empty() || msg.back() != '\n') {
            msg.push_back('\n');
        }
        auto data = std::make_shared<std::string>(std::move(msg));
        auto sock = socket;
        asio::async_write(*sock, asio::buffer(*data),
                           [data](const boost::system::error_code&, std::size_t) {});
    }
};

NetworkClient::NetworkClient(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {
    impl_->io = std::make_shared<asio::io_context>();
    impl_->work = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
        asio::make_work_guard(*impl_->io));
    impl_->thread = std::thread([this]() { impl_->runIo(); });
}

NetworkClient::~NetworkClient() {
    disconnectClient();
    if (impl_->work) {
        impl_->work.reset();
    }
    if (impl_->io) {
        impl_->io->stop();
    }
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
}

void NetworkClient::connectToServer(QString host, quint16 port) {
    asio::post(*impl_->io, [this, host = std::move(host), port]() {
        boost::system::error_code ec;
        tcp::resolver resolver(*impl_->io);
        auto endpoints = resolver.resolve(host.toStdString(), std::to_string(port), ec);
        if (ec) {
            emit errorReceived(QString::fromStdString(ec.message()));
            emit connectionChanged(false);
            return;
        }
        impl_->socket = std::make_shared<tcp::socket>(*impl_->io);
        asio::connect(*impl_->socket, endpoints, ec);
        if (ec) {
            emit errorReceived(QString::fromStdString(ec.message()));
            impl_->socket.reset();
            emit connectionChanged(false);
            return;
        }
        impl_->read_buf.consume(impl_->read_buf.size());
        emit connectionChanged(true);
        impl_->startRead();
    });
}

void NetworkClient::disconnectClient() {
    if (!impl_->io) {
        return;
    }
    std::promise<void> done;
    auto fut = done.get_future();
    asio::post(*impl_->io, [this, &done]() {
        if (impl_->socket) {
            boost::system::error_code ec;
            impl_->socket->shutdown(tcp::socket::shutdown_both, ec);
            impl_->socket->close(ec);
            impl_->socket.reset();
        }
        done.set_value();
    });
    fut.wait();
    emit connectionChanged(false);
}

void NetworkClient::createLobby() {
    asio::post(*impl_->io, [this]() { impl_->sendLine("CREATE_LOBBY"); });
}

void NetworkClient::joinLobby(QString inviteCode) {
    asio::post(*impl_->io, [this, code = std::move(inviteCode)]() mutable {
        impl_->sendLine("JOIN_LOBBY " + code.toStdString());
    });
}

void NetworkClient::placeTransmitters(int x1, int y1, int x2, int y2, int x3, int y3) {
    asio::post(*impl_->io, [this, x1, y1, x2, y2, x3, y3]() {
        std::ostringstream os;
        os << "PLACE_TRANSMITTERS " << x1 << ' ' << y1 << ' ' << x2 << ' ' << y2 << ' ' << x3 << ' ' << y3;
        impl_->sendLine(os.str());
    });
}

void NetworkClient::sendGradientStep() {
    asio::post(*impl_->io, [this]() { impl_->sendLine("GRADIENT_STEP"); });
}

void NetworkClient::moveReceiver(int idx, int x, int y) {
    asio::post(*impl_->io, [this, idx, x, y]() {
        std::ostringstream os;
        os << "MOVE_RECEIVER " << idx << ' ' << x << ' ' << y;
        impl_->sendLine(os.str());
    });
}

void NetworkClient::addReceiver(int x, int y) {
    asio::post(*impl_->io, [this, x, y]() {
        std::ostringstream os;
        os << "ADD_RECEIVER " << x << ' ' << y;
        impl_->sendLine(os.str());
    });
}

void NetworkClient::removeReceiver(int idx) {
    asio::post(*impl_->io, [this, idx]() {
        std::ostringstream os;
        os << "REMOVE_RECEIVER " << idx;
        impl_->sendLine(os.str());
    });
}

void NetworkClient::sendDone() {
    asio::post(*impl_->io, [this]() { impl_->sendLine("DONE"); });
}

} // namespace rssi_game::client
