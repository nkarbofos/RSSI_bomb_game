cmake_minimum_required(VERSION 3.16)

project(rssi_bomb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "$ENV{HOME}/boost-local")
set(BOOST_INCLUDEDIR "$ENV{HOME}/boost-local/include")
set(BOOST_LIBRARYDIR "$ENV{HOME}/boost-local/lib")

find_package(Boost REQUIRED COMPONENTS system)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

add_executable(client_demo
    demo/client_demo.cpp
)

target_link_libraries(client_demo
    PRIVATE
        Boost::system
)

