### This file is used for build example projects.

# set this will supress some warnings
set(BUILDING_SDK "yes" CACHE INTERNAL "")

# basic config
if (NOT PROJ)
    message(FATAL_ERROR "PROJ must be set. e.g. -DPROJ=hello_world")
else()
    message("PROJ = ${PROJ}")
endif ()
cmake_minimum_required(VERSION 3.0)
include(./cmake/common.cmake)
project(${PROJ})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_LIST_DIR}/lib/arch/include ${CMAKE_CURRENT_LIST_DIR}/lib/utils/include)
# config self use headers
header_directories(${SDK_ROOT}/lib)
# build library first
add_subdirectory(lib SDK)
add_subdirectory(third_party)

# compile project
add_source_files(src/${PROJ}/*.c src/${PROJ}/*.s src/${PROJ}/*.S src/${PROJ}/*.cpp)
include(./cmake/executable.cmake)