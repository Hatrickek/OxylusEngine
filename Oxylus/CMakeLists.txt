set(PROJECT_NAME Oxylus)

file(GLOB src "src/*.h" "src/*.cpp")
source_group("src" FILES ${src})
file(GLOB_RECURSE Assets "src/Assets/*.h" "src/Assets/*.cpp" )
source_group("src/Assets" FILES ${Assets})
file(GLOB_RECURSE Audio "src/Audio/*.h" "src/Audio/*.cpp" )
source_group("src/Audio" FILES ${Audio})
file(GLOB_RECURSE Core "src/Core/*.h" "src/Core/*.cpp" )
source_group("src/Core" FILES ${Core})
file(GLOB_RECURSE Event "src/Event/*.h" "src/Event/*.cpp" )
source_group("src/Event" FILES ${Event})
file(GLOB_RECURSE Physics "src/Physics/*.h" "src/Physics/*.cpp" )
source_group("src/Physics" FILES ${Physics})
file(GLOB_RECURSE Render "src/Render/*.h" "src/Render/*.cpp" )
source_group("src/Render" FILES ${Render})
file(GLOB_RECURSE Scene "src/Scene/*.h" "src/Scene/*.cpp" )
source_group("src/Scene" FILES ${Scene})
file(GLOB_RECURSE Thread "src/Thread/*.h" "src/Thread/*.cpp" )
source_group("src/Thread" FILES ${Thread})
file(GLOB_RECURSE UI "src/UI/*.h" "src/UI/*.cpp" )
source_group("src/UI" FILES ${UI})
file(GLOB_RECURSE Utils "src/Utils/*.h" "src/Utils/*.cpp" )
source_group("src/Utils" FILES ${Utils})

set(SOURCE_FILES ${src} ${Core} ${Assets} ${Audio} ${Event} ${Physics} ${Render} ${Scene} ${Thread} ${UI} ${Utils})

file(GLOB vendor__ImGui "vendor/ImGui/*.h" "vendor/ImGui/*.cpp")
source_group("vendor/ImGui" FILES ${vendor__ImGui})

set(vendor__ImGui__backends
  "vendor/ImGui/backends/imgui_impl_glfw.cpp"
  "vendor/ImGui/backends/imgui_impl_glfw.h"
  "vendor/ImGui/backends/imgui_impl_vulkan.cpp"
  "vendor/ImGui/backends/imgui_impl_vulkan.h"
  )
source_group("vendor/ImGui/backends" FILES ${vendor__ImGui__backends})

set(vendor__ImGui__misc__cpp "vendor/ImGui/misc/cpp/imgui_stdlib.cpp" "vendor/ImGui/misc/cpp/imgui_stdlib.h")
source_group("vendor/ImGui/misc/cpp" FILES ${vendor__ImGui__misc__cpp})

set(vendor__ImGuizmo "vendor/ImGuizmo/ImGuizmo.cpp" "vendor/ImGuizmo/ImGuizmo.h")
source_group("vendor/ImGuizmo" FILES ${vendor__ImGuizmo})

file(GLOB vendor__glm "vendor/glm/*.hpp")
source_group("vendor/glm" FILES ${vendor__glm})

set(vendor__miniaudio "vendor/miniaudio/miniaudio.h")
source_group("vendor/miniaudio" FILES ${vendor__miniaudio})

set(vendor__tracy__public "vendor/tracy/public/TracyClient.cpp")
source_group("vendor/tracy/public" FILES ${vendor__tracy__public})

set(ALL_FILES
  ${SOURCE_FILES}
  ${vendor__ImGui}
  ${vendor__ImGui__backends}
  ${vendor__ImGui__misc__cpp}
  ${vendor__ImGuizmo}
  ${vendor__glm}
  ${vendor__miniaudio}
  ${vendor__tracy__public}
  )

# Target
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_precompile_headers(${PROJECT_NAME} PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/oxpch.h>"
  )

set(ROOT_NAMESPACE Oxylus)

set_target_properties(${PROJECT_NAME} PROPERTIES
  VS_GLOBAL_KEYWORD "Win32Proj"
  )

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
  TARGET_NAME_DEBUG "Oxylus"
  TARGET_NAME_RELEASE "Oxylus"
  TARGET_NAME_Distribution "Oxylus"
  )
# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
  OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Debug-windows-x86_64/Oxylus/"
  OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Release-windows-x86_64/Oxylus/"
  OUTPUT_DIRECTORY_Distribution "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Distribution-windows-x86_64/Oxylus/"
  )

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW/include;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGui;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/entt;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo;"
  "${CMAKE_CURRENT_SOURCE_DIR}/.;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinygltf;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ktx/include;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public"
  )

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
  "$<$<CONFIG:Debug>:"
  "OX_DEBUG;"
  "_DEBUG;"
  "TRACY_ON_DEMAND"
  "TRACY_ENABLE"
  ">"
  "$<$<CONFIG:Release>:"
  "OX_RELEASE;"
  "TRACY_ON_DEMAND"
  "NDEBUG;"
  "TRACY_ENABLE"
  ">"
  "$<$<CONFIG:Distribution>:"
  "OX_Distribution;"
  "NDEBUG"
  ">"
  "VULKAN_HPP_NO_EXCEPTIONS"
  "VULKAN_HPP_NO_SPACESHIP_OPERATOR"
  "VULKAN_HPP_NO_TO_STRING"
  "_CRT_SECURE_NO_WARNINGS"
  "GLFW_INCLUDE_NONE"
  "_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS"
  "SPDLOG_NO_EXCEPTIONS"
  "_HAS_EXCEPTIONS=0;"
  "UNICODE"
  "_UNICODE"
  )

#-------------
# MSVC Specific
#-------------

# Compile and link options
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:
    /Od;
    /Zi
    >
    $<$<CONFIG:Release>:
    /Ox;
    /Oi;
    /GF;
    /Gy;
    /O2;
    /Zi;
    ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
    >
    $<$<CONFIG:Distribution>:
    /Oi;
    /GF;
    /Gy;
    /O2;
    ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
    >
    /MP;
    /std:c++latest;
    /Gm-;
    /W3
    /external:anglebrackets
    )
    target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:
        /DEBUG;
    >
    $<$<CONFIG:Release>:
        /DEBUG;
        /OPT:REF;
        /OPT:ICF;
    >
    $<$<CONFIG:Distribution>:
        /OPT:REF;
        /OPT:ICF;
    >
    /SUBSYSTEM:CONSOLE
    )
    set(CMAKE_SHARED_LINKER_FLAGS_DISTRIBUTION ${CMAKE_SHARED_LINKER_FLAGS})
    set(CMAKE_EXE_LINKER_FLAGS_DISTRIBUTION ${CMAKE_EXE_LINKER_FLAGS})
endif ()

# MSVC runtime library

get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
  $<$<CONFIG:Debug>:
  MultiThreadedDebug
  >
  $<$<CONFIG:Release>:
  MultiThreaded
  >
  $<$<CONFIG:Distribution>:
  MultiThreaded
  >
  $<$<NOT:$<OR:$<CONFIG:Debug>,
  $<CONFIG:Distribution>,
  $<CONFIG:Release>
  >>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
  )
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

#-------------
# Dependencies
#-------------
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/CMake-Utils/CMakeUtils.cmake)

# Suppress all warnings from dependencies.
if(MSVC)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /W0)
else()
    set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS -w)
endif()

include(FetchContent)

# Vulkan Headers
FetchContent_Declare(
        Vulkan-Headers
        GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git"
        GIT_TAG "v1.3.239"
)
FetchContent_MakeAvailable(Vulkan-Headers)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan-Headers)
get_target_property(Vulkan_HEADERS_SOURCEDIR Vulkan-Headers SOURCE_DIR)

# Vulkan
find_package(Vulkan)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

# VMA
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/VulkanMemoryAllocator/include")

# spirv-headers
set(PROJECT_IS_TOP_LEVEL OFF)
set(BUILD_TESTS OFF)
FetchContent_Declare(
        SPIRV-Headers
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git"
        GIT_TAG "sdk-1.3.243.0"
)
FetchContent_MakeAvailable(SPIRV-Headers)
get_target_property(SPIRV_HEADER_DIR SPIRV-Headers SOURCE_DIR)
set(SPIRV_HEADERS_DIR ${SPIRV-Headers_SOURCE_DIR})
set_targets_static_and_folder(SPIRV-Headers)

# spirv-tools
set(SPIRV_SKIP_TESTS ON)
set(SPIRV_SKIP_EXECUTABLES ON)
FetchContent_Declare(
        SPIRV-Tools
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git"
        GIT_TAG "v2023.2"
)
FetchContent_MakeAvailable(SPIRV-Tools)
get_target_property(SPIRV_TOOLS_DIR SPIRV-Tools SOURCE_DIR)
set_targets_static_and_folder(SPIRV-Tools)
FOLDER_DIR_TARGETS(spirv-tools-pkg-config)
FOLDER_DIR_TARGETS(spirv-tools-shared-pkg-config)

# glslang
set(ENABLE_CTEST OFF)
set(ENABLE_GLSLANG_BINARIES OFF)
set(SKIP_GLSLANG_INSTALL ON)
set(BUILD_EXTERNAL OFF)
FetchContent_Declare(
        glslang
        GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git"
        GIT_TAG "12.1.0"
)
FetchContent_MakeAvailable(glslang)
get_target_property(GLSL_SOURCE_DIR glslang SOURCE_DIR)
set_targets_static_and_folder(glslang)
set_targets_static_and_folder(SPIRV)
set_targets_static_and_folder(OGLCompiler)
set_targets_static_and_folder(SPVRemapper)
set_targets_static_and_folder(HLSL)

# shaderc
set(SHADERC_SKIP_INSTALL ON)
set(SHADERC_ENABLE_SHARED_CRT OFF)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_EXAMPLES ON)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON)    
set(SHADERC_GLSLANG_DIR ${GLSL_SOURCE_DIR})
FetchContent_Declare(
        shaderc
        GIT_REPOSITORY "https://github.com/google/shaderc.git"
        GIT_TAG "v2023.3"
)
FetchContent_MakeAvailable(shaderc)
target_link_libraries(${PROJECT_NAME} PUBLIC shaderc)
get_target_property(SHADERC_SOURCE_DIR shaderc SOURCE_DIR)
set_targets_static_and_folder(shaderc)
set_targets_static_and_folder(shaderc_util)
FOLDER_DIR_TARGETS(add-copyright)
FOLDER_DIR_TARGETS(build-version)
FOLDER_DIR_TARGETS(glslc)
set_targets_static_and_folder(glslc_exe)
FOLDER_DIR_TARGETS(shaderc_static-pkg-config)
FOLDER_DIR_TARGETS(shaderc-pkg-config)
FOLDER_DIR_TARGETS(shaderc_combined-pkg-config)

# NFD
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/nativefiledialog-extended)
target_link_libraries(${PROJECT_NAME} PUBLIC nfd)
set_targets_static_and_folder(nfd)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
set_targets_static_and_folder(glfw)
FOLDER_DIR_TARGETS(uninstall)
disable_ipo_target(glfw)

# RYML
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/ryml)
target_link_libraries(${PROJECT_NAME} PUBLIC ryml::ryml)
set_target_properties(ryml PROPERTIES  FOLDER "Vendor")
target_link_libraries(${PROJECT_NAME} PUBLIC c4core)
set_targets_static_and_folder(ryml)

# KTX
set(CLI OFF)
set(KTX_FEATURE_TOOLS OFF)
set(KTX_FEATURE_TESTS OFF)
set(KTX_FEATURE_GL_UPLOAD OFF)
set(KTX_FEATURE_STATIC_LIBRARY ON)
set(BASISU_SUPPORT_OPENCL OFF)
FetchContent_Declare(
        ktx
        GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software"
        GIT_TAG "v4.1.0"
)
FetchContent_MakeAvailable(ktx)
target_link_libraries(${PROJECT_NAME} PUBLIC ktx)
set_targets_static_and_folder(ktx)
disable_ipo_target(ktx)

# FMT
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG master)
FetchContent_MakeAvailable(fmt)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt)
set_targets_static_and_folder(fmt)

# Jolt
set(COMPILE_AS_SHARED_LIBRARY OFF)
set(ENABLE_ALL_WARNINGS OFF)
set(INTERPROCEDURAL_OPTIMIZATION ON)
FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
        GIT_TAG "v3.0.1"
		SOURCE_SUBDIR "Build"
)
FetchContent_MakeAvailable(JoltPhysics)
target_link_libraries(${PROJECT_NAME} PUBLIC Jolt)
target_include_directories(${PROJECT_NAME} PUBLIC ${JoltPhysics_SOURCE_DIR}/..)
FOLDER_DIR_TARGETS(Jolt)
