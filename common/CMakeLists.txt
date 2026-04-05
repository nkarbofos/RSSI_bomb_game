cmake_minimum_required(VERSION 3.16)

project(rssi_bomb_common LANGUAGES CXX)

add_library(rssi_bomb_common STATIC protocol_parser.cpp)

target_include_directories(rssi_bomb_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

