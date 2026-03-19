cmake_minimum_required(VERSION 3.16)

project(rssi_bomb_client LANGUAGES CXX)

add_library(rssi_bomb_client STATIC
    main_window.hpp
    network_client.hpp
    dummy.cpp
)

target_include_directories(rssi_bomb_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

