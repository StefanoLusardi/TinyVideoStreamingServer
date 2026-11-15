cmake_minimum_required(VERSION 3.20)
project(mjpeg_proxy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MJPEGPROXY_WARNINGS_AS_ERRORS "Tratta i warning come errori" OFF)

find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_library(mjpegproxy STATIC)
add_library(mjpegproxy::mjpegproxy ALIAS mjpegproxy)

set(PublicHeaders
    include/mjpegproxy/CameraSpec.hpp
    include/mjpegproxy/Endpoint.hpp
    include/mjpegproxy/Server.hpp
    include/mjpegproxy/Settings.hpp
)

set(Sources
    src/Camera.cpp
    src/CameraRegistry.cpp
    src/CameraSpec.cpp
    src/Endpoint.cpp
    src/FrameHub.cpp
    src/HtmlPages.cpp
    src/HttpMjpegSource.cpp
    src/HttpServer.cpp
    src/Log.cpp
    src/MultipartSplitter.cpp
    src/Route.cpp
    src/Server.cpp
    src/Settings.cpp
)

target_sources(mjpegproxy PUBLIC ${PublicHeaders} PRIVATE ${Sources})
target_compile_features(mjpegproxy PUBLIC cxx_std_20)
target_link_libraries(mjpegproxy PRIVATE Boost::headers Threads::Threads)
target_include_directories(mjpegproxy
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(mjpegproxy PRIVATE /W4 $<$<BOOL:${MJPEGPROXY_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(mjpegproxy PRIVATE -Wall -Wextra -Wpedantic
        $<$<BOOL:${MJPEGPROXY_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

add_executable(mjpeg-proxy app/main.cpp)
target_link_libraries(mjpeg-proxy PRIVATE mjpegproxy::mjpegproxy)
install(TARGETS mjpeg-proxy RUNTIME DESTINATION bin)
