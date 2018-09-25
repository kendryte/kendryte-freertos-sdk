include(${CMAKE_CURRENT_LIST_DIR}/reset.cmake)

if (NOT BUILDING_SDK)
    if(EXISTS ${SDK_ROOT}/libkendryte.a)
        ### compiled sdk
        header_directories(${SDK_ROOT}/include)
        add_library(kendryte STATIC IMPORTED)
        set_property(TARGET kendryte PROPERTY IMPORTED_LOCATION ${SDK_ROOT}/libkendryte.a)
    else()
        ### source code sdk
        header_directories(${SDK_ROOT}/lib)
        add_subdirectory(${SDK_ROOT}/lib SDK)
    endif()
endif ()

removeDuplicateSubstring(${CMAKE_C_FLAGS} CMAKE_C_FLAGS)
removeDuplicateSubstring(${CMAKE_CXX_FLAGS} CMAKE_CXX_FLAGS)

message("SOURCE_FILES=${SOURCE_FILES}")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)

target_link_libraries(${PROJECT_NAME}
        -Wl,--start-group
        m kendryte
        -Wl,--end-group
        )

IF(SUFFIX)
    SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SUFFIX ${SUFFIX})
ENDIF()

# Build target
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --output-format=binary ${CMAKE_BINARY_DIR}/${PROJECT_NAME}${SUFFIX} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
        DEPENDS ${PROJECT_NAME}
        COMMENT "Generating .bin file ...")

# show information
include(${CMAKE_CURRENT_LIST_DIR}/dump-config.cmake)