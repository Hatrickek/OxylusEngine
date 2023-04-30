cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(Oxylus CXX C)
set(CMAKE_CXX_STANDARD 20)
set(INTERPROCEDURAL_OPTIMIZATION ON)

# Set target arch type if empty.
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

# Global configuration types
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Distribution"
    "Release"
    CACHE STRING "" FORCE
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Sub-projects
add_subdirectory(Oxylus)
add_subdirectory(OxylusEditor)

