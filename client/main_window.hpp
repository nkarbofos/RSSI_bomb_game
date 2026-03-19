
#pragma once

#include <memory>
//#include <QMainWindow>

#include "common/game_types.hpp"

namespace rssi_game::client {

class NetworkClient;

class MainWindow /* : public QMainWindow */ {
    // Q_OBJECT
public:
    explicit MainWindow(std::shared_ptr<NetworkClient> network_client);
    void setupUi();
    void updateGameState();
    void updateRssiView();

private:
    std::shared_ptr<NetworkClient> network_client_;

    GridSize grid_size_{};
};

} // namespace rssi_game::client

