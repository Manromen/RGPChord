cmake_minimum_required (VERSION 2.8)
project (RGP-Log)

# The version number: 0.1
set (RGP_Log_VERSION_MAJOR 0)
set (RGP_Log_VERSION_MINOR 1)

# enable c++11
if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -stdlib=libc++")
endif()
# MSVC does not require any special flags for c++11 support

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# create library
add_library(rgp-log STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/RGP-Log.cpp)

# create test executable
add_executable(test
               ${CMAKE_CURRENT_SOURCE_DIR}/test/test.cpp)

# link library to test executable
target_link_libraries(test rgp-log)

# installation 
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/RGP-Log.h
  DESTINATION include)
INSTALL(TARGETS rgp-log
  ARCHIVE DESTINATION lib)
