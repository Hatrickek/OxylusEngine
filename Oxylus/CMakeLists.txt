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

file(GLOB vendor__ImGui "vendor/imgui/*.h" "vendor/imgui/*.cpp")
source_group("vendor/imgui" FILES ${vendor__ImGui})

set(vendor__ImGui__backends
  "vendor/imgui/backends/imgui_impl_glfw.cpp"
  "vendor/imgui/backends/imgui_impl_glfw.h"
  "vendor/imgui/backends/imgui_impl_vulkan.cpp"
  "vendor/imgui/backends/imgui_impl_vulkan.h"
  )
source_group("vendor/imgui/backends" FILES ${vendor__ImGui__backends})

set(vendor__ImGui__misc__cpp "vendor/imgui/misc/cpp/imgui_stdlib.cpp" "vendor/imgui/misc/cpp/imgui_stdlib.h")
source_group("vendor/imgui/misc/cpp" FILES ${vendor__ImGui__misc__cpp})

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
  TARGET_NAME_DISTRIBUTION "Oxylus"
  TARGET_NAME_RELEASE "Oxylus"
  )
# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
  OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Debug-windows-x86_64/Oxylus/"
  OUTPUT_DIRECTORY_DISTRIBUTION "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Distribution-windows-x86_64/Oxylus/"
  OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Release-windows-x86_64/Oxylus/"
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
  "$<$<CONFIG:Distribution>:"
  "OX_DIST;"
  "NDEBUG"
  ">"
  "$<$<CONFIG:Release>:"
  "OX_RELEASE;"
  "TRACY_ON_DEMAND"
  "NDEBUG;"
  "TRACY_ENABLE"
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
    $<$<CONFIG:Distribution>:
    /Ox;
    /Oi;
    /GF;
    /Gy;
    ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
    >
    $<$<CONFIG:Release>:
    /Ox;
    /Oi;
    /GF;
    /Gy;
    ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
    >
    /MP;
    /std:c++latest;
    /Gm-;
    /W3
    )
  target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Distribution>:
    /OPT:REF;
    /OPT:ICF
    >
    $<$<CONFIG:Release>:
    /OPT:REF;
    /OPT:ICF
    >
    /SUBSYSTEM:WINDOWS
    )
endif ()

# MSVC runtime library

get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
  $<$<CONFIG:Debug>:
  MultiThreadedDebug
  >
  $<$<CONFIG:Distribution>:
  MultiThreaded
  >
  $<$<CONFIG:Release>:
  MultiThreaded
  >
  $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Distribution>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
  )
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

#-------------
# Dependencies
#-------------
function(get_all_targets var dir)
    set(targets)
    get_all_targets_recursive(targets ${dir})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

function(folder_dir_targets _target)
    get_target_property(TARGET_SOURCE_DIR ${_target} SOURCE_DIR)
    get_all_targets(targets ${TARGET_SOURCE_DIR})
    set_target_properties(${targets} PROPERTIES FOLDER "Vendor/${_target}")
endfunction()

function(set_targets_static _target)
    get_target_property(TARGET_SOURCE_DIR ${_target} SOURCE_DIR)
    get_all_targets(targets ${TARGET_SOURCE_DIR})
    set_target_properties(${targets} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
endfunction()

function(set_targets_static_and_folder _target)
    get_target_property(TARGET_SOURCE_DIR ${_target} SOURCE_DIR)
    get_all_targets(targets ${TARGET_SOURCE_DIR})
    set_target_properties(${targets} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR} FOLDER "Vendor/${_target}")
endfunction()

# Suppress all warnings from dependencies.
if(MSVC)
set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /W0)
else()
set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS -w)
endif()

include(FetchContent)

# Set build type (This is actually not required)
set(CMAKE_BUILD_TYPE Release)

# Vulkan Headers
FetchContent_Declare(
        Vulkan-Headers
        GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git"
        GIT_TAG "v1.3.239"
)
FetchContent_MakeAvailable(Vulkan-Headers)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan-Headers)
get_target_property(Vulkan_HEADERS_SOURCEDIR Vulkan-Headers SOURCE_DIR)

# Vulkan dll
find_package(Vulkan)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

# VMA
set(VMA_STATIC_VULKAN_FUNCTIONS OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON)
FetchContent_Declare(
        vma
        GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
        GIT_TAG "v3.0.1"
)
FetchContent_MakeAvailable(vma)
target_include_directories(${PROJECT_NAME} PUBLIC ${vma_SOURCE_DIR}/include)

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

# RYML
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/ryml)
target_link_libraries(${PROJECT_NAME} PUBLIC ryml::ryml)
set_target_properties(ryml PROPERTIES  FOLDER "Vendor")
target_link_libraries(${PROJECT_NAME} PUBLIC c4core)
set_targets_static_and_folder(ryml)

# KTX
set(KTX_FEATURE_TESTS OFF)
set(KTX_FEATURE_GL_UPLOAD OFF)
set(KTX_FEATURE_STATIC_LIBRARY ON)
set(BASISU_SUPPORT_OPENCL OFF)
set(BASISU_SUPPORT_SSE OFF)
FetchContent_Declare(
        ktx
        GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software"
        GIT_TAG "v4.1.0"
)
FetchContent_MakeAvailable(ktx)
target_link_libraries(${PROJECT_NAME} PUBLIC ktx)
set_targets_static_and_folder(ktx)

# FMT
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG master)
FetchContent_MakeAvailable(fmt)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt)
set_targets_static_and_folder(fmt)

# Jolt
FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
        GIT_TAG "v3.0.1"
		SOURCE_SUBDIR "Build"
)
FetchContent_MakeAvailable(JoltPhysics)
target_link_libraries(${PROJECT_NAME} PUBLIC Jolt)
target_include_directories(${PROJECT_NAME} PUBLIC ${JoltPhysics_SOURCE_DIR}/..)
set_targets_static_and_folder(Jolt)