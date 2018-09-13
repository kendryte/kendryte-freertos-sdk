
message("CMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
message("_TC_MAKE=${_TC_MAKE}")
if (_TC_MAKE)
    global_set(CMAKE_MAKE_PROGRAM "${_TC_MAKE}")
endif()

if (NOT BUILDING_SDK)
    if(EXISTS ${SDK_ROOT}/libkendryte.a)
        header_directories(${SDK_ROOT}/include)
        add_library(kendryte STATIC IMPORTED)
        set_property(TARGET kendryte PROPERTY IMPORTED_LOCATION ${SDK_ROOT}/libkendryte.a)
    else()
        header_directories(${SDK_ROOT}/lib)
        add_subdirectory(${SDK_ROOT}/lib SDK)
    endif()
endif ()

removeDuplicateSubstring(${CMAKE_C_FLAGS} CMAKE_C_FLAGS)
removeDuplicateSubstring(${CMAKE_CXX_FLAGS} CMAKE_CXX_FLAGS)

message("SOURCE_FILES=${SOURCE_FILES}")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
# add_dependencies(${PROJECT_NAME} kendryte) # TODO: third_party
# target_link_libraries(${PROJECT_NAME} kendryte) # TODO: third_party
# link_directories(${CMAKE_BINARY_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)

EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crt0.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRT0_OBJ)
EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crtbegin.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTBEGIN_OBJ)
EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crtend.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTEND_OBJ)
EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crti.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTI_OBJ)
EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crtn.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTN_OBJ)

set(CMAKE_C_LINK_EXECUTABLE
"<CMAKE_C_COMPILER>  <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> ${CRTI_OBJ} ${CRTBEGIN_OBJ} <OBJECTS> ${CRTEND_OBJ} ${CRTN_OBJ} -o <TARGET> <LINK_LIBRARIES>")

set(CMAKE_CXX_LINK_EXECUTABLE
"<CMAKE_CXX_COMPILER>  <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> ${CRTI_OBJ} ${CRTBEGIN_OBJ} <OBJECTS> ${CRTEND_OBJ} ${CRTN_OBJ} -o <TARGET> <LINK_LIBRARIES>")

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