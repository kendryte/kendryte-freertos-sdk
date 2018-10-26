PROJECT(fatfs)

SET(FATFS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

SET(fatfs_SRCS
    ${FATFS_DIR}/source/ff.c
    ${FATFS_DIR}/source/ffsystem.c
    ${FATFS_DIR}/source/ffunicode.c
)

INCLUDE_DIRECTORIES(${SDK_ROOT}/lib/freertos/include ${SDK_ROOT}/lib/freertos/conf ${SDK_ROOT}/lib/freertos/portable)
ADD_LIBRARY(fatfs STATIC ${fatfs_SRCS})

TARGET_INCLUDE_DIRECTORIES(fatfs PUBLIC ${FATFS_DIR}/source)
