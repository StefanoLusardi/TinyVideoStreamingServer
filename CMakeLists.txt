cmake_minimum_required(VERSION 3.20)
project(mjpeg_proxy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)   # solo header: Asio + Beast
find_package(Threads REQUIRED)

add_executable(mjpeg-proxy src/main.cpp)
target_link_libraries(mjpeg-proxy PRIVATE Boost::headers Threads::Threads)
target_compile_options(mjpeg-proxy PRIVATE -Wall -Wextra)
