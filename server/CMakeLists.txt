cmake_minimum_required(VERSION 3.16)

project(rssi_bomb_server LANGUAGES CXX)

add_library(rssi_bomb_server STATIC
    game_session.hpp
    game_session.cpp
    game_manager.hpp
    game_manager.cpp
    network_server.hpp
    network_server.cpp
    dummy.cpp
)

target_include_directories(rssi_bomb_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(rssi_bomb_server
    PUBLIC
        Boost::system
        rssi_bomb_common
)

add_executable(rssi_bomb_server_app
    main.cpp
)

target_link_libraries(rssi_bomb_server_app
    PRIVATE
        rssi_bomb_server
)

add_executable(server_tests
    ../tests/server_tests.cpp
)

target_link_libraries(server_tests
    PRIVATE
        rssi_bomb_server
        Boost::system
)

