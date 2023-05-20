cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(Oxylus CXX C)
set(CMAKE_CXX_STANDARD 20)
# Enable IPO with its policy
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
cmake_policy(SET CMP0069 NEW) 
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

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
    "Release"
    "Distribution"
    CACHE STRING "" FORCE
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Sub-projects
add_subdirectory(Oxylus)
add_subdirectory(OxylusEditor)
add_subdirectory(OxylusRuntime)

