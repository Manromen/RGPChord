cmake_minimum_required (VERSION 2.8)
project (RGP-Chord)

# The version number: 0.1
set (RGP_Chord_VERSION_MAJOR 0)
set (RGP_Chord_VERSION_MINOR 1)

# enable c++11
if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -stdlib=libc++")
endif()
# MSVC does not require any special flags for c++11 support

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# create library
add_library(rgp-chord STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Chord.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ChordData.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ChordNode.cpp)

# create test executable
add_executable(example
               ${CMAKE_CURRENT_SOURCE_DIR}/example/example.cpp)

# link library to test executable
target_link_libraries(example rgp-chord)

# link external libraries
find_library(rgp-log librgp-log.a)

if(${rgp-log} STREQUAL "rgp-log-NOTFOUND")
    message(FATAL_ERROR "Couldn't find the 'external_lib' library")
endif()
message(STATUS "Found 'RGP-Log' at: ${rgp-log}")

target_link_libraries(rgp-chord rgp-log)
target_link_libraries(example rgp-log)

# installation 
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/rgp
  DESTINATION include)

INSTALL(TARGETS rgp-chord
  ARCHIVE DESTINATION lib)
