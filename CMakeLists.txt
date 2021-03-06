cmake_minimum_required (VERSION 2.8)
project (RGPChord)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3")

message(STATUS "Build Type is ${CMAKE_BUILD_TYPE}")

# check compiler for c++11 capability
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") # Clang
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.3")
    message(WARNING "Your compiler version may not support all used C++11 features!")
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") # GCC
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8.1")
    message(WARNING "Your compiler version may not support all used C++11 features!")
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel") # Intel
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
    message(WARNING "Your compiler version may not support all used C++11 features!")
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC") # MS Visual Compiler
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.0")
    message(WARNING "Your compiler version may not support all used C++11 features!")
  endif()
endif()

# enable c++11
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -stdlib=libc++")
endif()
# MSVC does not require any special flags for c++11 support

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# create library
add_library(rgpchord SHARED
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Chord.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ChordNode.cpp)

# create example executable
add_executable(example
               ${CMAKE_CURRENT_SOURCE_DIR}/example/example.cpp)

# link library to example executable
target_link_libraries(example rgpchord)

# link external libraries
find_library(rgputils NAMES rgputils)

if(${rgplog} STREQUAL "rgputils-NOTFOUND")
    message(FATAL_ERROR "Couldn't find the RGPUtils library. Please check the Readme.md for required dependencies.")
endif()
message(STATUS "Found 'RGPUtils' at: ${rgputils}")

target_link_libraries(rgpchord rgputils)
target_link_libraries(example rgputils)

# set version info
set_target_properties(rgpchord PROPERTIES
					  VERSION 0.1
					  SOVERSION 0)

# installation 
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/rgp
  DESTINATION include)

install(TARGETS rgpchord
  LIBRARY DESTINATION lib)
