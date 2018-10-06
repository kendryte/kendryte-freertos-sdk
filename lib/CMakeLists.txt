#project(kendryte_drivers)

# create driver library
FILE(GLOB_RECURSE LIB_SRC_NOASM
        "${CMAKE_CURRENT_LIST_DIR}/*.h"
        "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/*.c"
        )

FILE(GLOB_RECURSE ASSEMBLY_FILES
        "${CMAKE_CURRENT_LIST_DIR}/*.s"
        "${CMAKE_CURRENT_LIST_DIR}/*.S"
        )
SET(LIB_SRC ${LIB_SRC_NOASM} ${ASSEMBLY_FILES})

include_directories(${CMAKE_CURRENT_LIST_DIR}/drivers/include ${CMAKE_CURRENT_LIST_DIR}/bsp/include)
#
#HEADER_DIRECTORIES(LIB_HEADERS)
#
#INCLUDE_DIRECTORIES(${LIB_HEADERS})

SET_PROPERTY(SOURCE ${ASSEMBLY_FILES} PROPERTY LANGUAGE C)
SET_SOURCE_FILES_PROPERTIES(${ASSEMBLY_FILES} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp -D __riscv64")

#MESSAGE("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

ADD_LIBRARY(kendryte STATIC ${LIB_SRC})
SET_TARGET_PROPERTIES(kendryte PROPERTIES LINKER_LANGUAGE C)

# find headers files to INSTALL
file(GLOB_RECURSE PRJ_FREERTOS_CONFIG "${PROJECT_SOURCE_DIR}/*FreeRTOSConfig.h")
if(PRJ_FREERTOS_CONFIG)
        file(GLOB_RECURSE LIB_HEADERS
                "../lib/*.h"
                "../lib/*.hpp"
                "${PROJECT_SOURCE_DIR}/*FreeRTOSConfig.h"
                )
        list(FILTER LIB_HEADERS EXCLUDE REGEX ".*${CMAKE_CURRENT_LIST_DIR}/\\.\\./lib/freertos/conf/FreeRTOSConfig\\.h.*")
else()
        file(GLOB_RECURSE LIB_HEADERS
                "../lib/*.h"
                "../lib/*.hpp"
                )
endif()
set_target_properties(kendryte PROPERTIES PUBLIC_HEADER "${LIB_HEADERS}")

# copy .a file and headers
install(TARGETS kendryte
        EXPORT kendryte
        ARCHIVE
        DESTINATION ${CMAKE_BINARY_DIR}/archive
        PUBLIC_HEADER DESTINATION ${CMAKE_BINARY_DIR}/archive/include
        )
