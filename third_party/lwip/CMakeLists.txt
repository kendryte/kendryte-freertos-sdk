cmake_minimum_required(VERSION 3.7)

project(lwIP)

set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LWIP_DIR}/src/include)
include(src/Filelists.cmake)