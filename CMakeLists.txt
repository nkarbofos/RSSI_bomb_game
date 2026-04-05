cmake_minimum_required(VERSION 3.16)

project(rssi_bomb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

include(FetchContent)
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

set(BOOST_ROOT "$ENV{HOME}/boost-local")
set(BOOST_INCLUDEDIR "$ENV{HOME}/boost-local/include")
set(BOOST_LIBRARYDIR "$ENV{HOME}/boost-local/lib")

find_package(Boost REQUIRED COMPONENTS system)

# Qt6 (клиент и client_tests): дополнительный путь к префиксу установки Qt (альтернатива -DCMAKE_PREFIX_PATH).
if(DEFINED ENV{QT6_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH "$ENV{QT6_PREFIX_PATH}" ${CMAKE_PREFIX_PATH})
endif()

enable_testing()

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

