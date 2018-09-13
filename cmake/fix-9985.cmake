### http://www.cmake.org/Bug/view.php?id=9985
string(REPLACE "-rdynamic" "" _C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
string(REPLACE "-rdynamic" "" _CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")
global_set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${_C_FLAGS}")
global_set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${_CXX_FLAGS}")