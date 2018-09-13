### This file is used for build library standalone.

# set this will supress some warnings
set(BUILDING_SDK "yes" CACHE INTERNAL "")

# basic config
cmake_minimum_required(VERSION 3.0)
include(./common.cmake)
project(kendryte)

# config self use headers
header_directories(../lib)
# build library first
add_subdirectory(../lib SDK)

# copy utils files
install(DIRECTORY
        ../lds
        ../utils
        ../cmake
        DESTINATION ${CMAKE_BINARY_DIR}/archive
        PATTERN "CMakeLists.txt" EXCLUDE
        PATTERN ".c" EXCLUDE
        )
install(DIRECTORY
        ../lib
        DESTINATION ${CMAKE_BINARY_DIR}/archive
        )

# show information
include(./dump-config.cmake)
