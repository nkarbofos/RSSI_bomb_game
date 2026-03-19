cmake_minimum_required(VERSION 3.16)

project(rssi_bomb_common LANGUAGES CXX)

add_library(rssi_bomb_common INTERFACE)

target_include_directories(rssi_bomb_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

