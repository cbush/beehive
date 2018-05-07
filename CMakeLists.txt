cmake_minimum_required(VERSION 3.4)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

# This CMakeLists adapted from nlohmann's json library.

project(beehive VERSION 0.0.1 LANGUAGES CXX)

include(ExternalProject)

option(BEEHIVE_BuildTests "Build the unit tests when BUILD_TESTING is enabled." OFF)

set(BEEHIVE_TARGET_NAME ${PROJECT_NAME})
set(BEEHIVE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/beehive)

add_library(${BEEHIVE_TARGET_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${BEEHIVE_TARGET_NAME} ALIAS ${BEEHIVE_TARGET_NAME})
target_compile_features(${BEEHIVE_TARGET_NAME} INTERFACE cxx_std_14)

target_include_directories(${BEEHIVE_TARGET_NAME}
    INTERFACE
    $<BUILD_INTERFACE:${BEEHIVE_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:beehive>
)

include(CTest) # adds option BUILD_TESTING (default ON)

if (BUILD_TESTING AND BEEHIVE_BuildTests)
    enable_testing()
    add_subdirectory(test)
endif()
