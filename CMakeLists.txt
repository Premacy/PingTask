cmake_minimum_required(VERSION 3.2)

set(PROJECT_NAME PingStatistic)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -O3")

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

set(SOURCES src/main.cpp)
set(PATH_TO_LIBS ${CMAKE_SOURCE_DIR}/Libs )
set(SPDLOG_LIB ${CMAKE_SOURCE_DIR}/Libs/spdlog/libspdlog.a)

set(LIBS ${SPDLOG_LIB})

include_directories(${PATH_TO_DIRS})
include_directories(${PATH_TO_LIBS})
include_directories(${PATH_TO_LIBS}/spdlog/include)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} ${LIBS})