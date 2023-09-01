cmake_minimum_required(VERSION 3.12)

project(LockFreeQueue LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})


add_executable(a.out testArrayCasQueue.cpp)
