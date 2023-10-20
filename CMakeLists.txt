cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

project(Oxylus CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable IPO with its policy
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(CMAKE_VS_PLATFORM_NAME "x64")

# Global configuration types
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    "Distribution"
    CACHE STRING "" FORCE
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ASAN
if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# Sub-projects

# fix for the new stupid 'set' issue
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# fix for dxc
if (UNIX)
    add_compile_options(-fms-extensions)
endif(UNIX)

add_subdirectory(Oxylus)
add_subdirectory(OxylusEditor)
add_subdirectory(OxylusRuntime)

