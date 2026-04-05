cmake_minimum_required(VERSION 3.16)

project(rssi_bomb_client_ui LANGUAGES CXX)

find_package(Qt6 QUIET COMPONENTS Quick QuickControls2 Core Gui Qml Test)

if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    qt_add_executable(rssi_bomb_client
        main.cpp
        network_client.cpp
        game_controller.cpp
        resources.qrc
    )

    target_include_directories(rssi_bomb_client
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    target_link_libraries(rssi_bomb_client
        PRIVATE
            Qt6::Quick
            Qt6::QuickControls2
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            rssi_bomb_common
            Boost::system
    )

    add_executable(client_tests
        ../tests/client_tests.cpp
    )

    set_target_properties(client_tests PROPERTIES AUTOMOC ON)

    target_include_directories(client_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    target_link_libraries(client_tests
        PRIVATE
            Qt6::Test
            Qt6::Core
            rssi_bomb_common
    )

    add_test(NAME client_tests COMMAND client_tests)
else()
    message(STATUS "Qt6 not found — skipping rssi_bomb_client and client_tests (set CMAKE_PREFIX_PATH to Qt 6)")
endif()
