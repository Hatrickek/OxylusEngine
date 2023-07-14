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
  ${vendor__ImGui__backends}
  ${vendor__ImGui__misc__cpp}
  ${vendor__ImGuizmo}
  ${vendor__glm}
  ${vendor__miniaudio}
  ${vendor__tracy__public}
  )

# Target
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

set(ROOT_NAMESPACE Oxylus)

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
  TARGET_NAME_DEBUG "Oxylus"
  TARGET_NAME_RELEASE "Oxylus"
  TARGET_NAME_Distribution "Oxylus"
  )

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW/include;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGui;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo;"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinygltf;"
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
  "OX_DISTRIBUTION;"
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

# Vulkan
if (VULKAN_SDK)
    message("Within Oxylus CMake, setting VULKAN_SDK env var to " ${VULKAN_SDK})
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

# shaderc from sdk
if(NOT SHADERC_FROM_SOURCE)
    add_library(shaderc UNKNOWN IMPORTED)
    if(WIN32)
        # use the version in the SDK
        set_target_properties(shaderc PROPERTIES IMPORTED_LOCATION $ENV{VULKAN_SDK}/Lib/shaderc_shared.lib)
        set_property(TARGET shaderc PROPERTY INTERFACE_INCLUDE_DIRECTORIES $ENV{VULKAN_SDK}/Include)
        target_link_libraries(${PROJECT_NAME} PRIVATE shaderc)
    else()
        target_link_directories(${PROJECT_NAME} PUBLIC $ENV{VULKAN_SDK}/lib)
        target_link_libraries(${PROJECT_NAME} PRIVATE shaderc_combined)
    endif()
endif()

# dear imgui
FetchContent_Declare(
    ImGui
    GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
    GIT_TAG "docking"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
set(IMGUI_DIR "${FETCHCONTENT_BASE_DIR}/imgui-src")
FetchContent_MakeAvailable(ImGui)
target_include_directories(${PROJECT_NAME} PUBLIC ${IMGUI_DIR})

file(GLOB ImGuiFiles "${IMGUI_DIR}/*.h" "${IMGUI_DIR}/*.cpp")
target_sources(${PROJECT_NAME} PUBLIC "${ImGuiFiles}")

set(ImGui_Backends
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.h"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.h"
)
target_sources(${PROJECT_NAME} PUBLIC "${ImGui_Backends}")

set(ImGui_Misc "${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp" "${IMGUI_DIR}/misc/cpp/imgui_stdlib.h")
target_sources(${PROJECT_NAME} PUBLIC "${ImGui_Misc}")

# entt
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY "https://github.com/skypjack/entt.git"
    GIT_TAG "master"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(EnTT)
target_link_libraries(${PROJECT_NAME} PUBLIC EnTT)
set_targets_static_and_folder(EnTT)

# VMA
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/VulkanMemoryAllocator/include")

# spirv-reflect
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    spirv-reflect
    GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git"
    GIT_TAG "main"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(spirv-reflect)
get_target_property(SPIRV_REFLECT_DIR spirv-reflect-static SOURCE_DIR)
target_include_directories(${PROJECT_NAME} PUBLIC ${SPIRV_REFLECT_DIR})
target_sources(${PROJECT_NAME} PUBLIC "${SPIRV_REFLECT_DIR}/spirv_reflect.cpp")
set_targets_static_and_folder(spirv-reflect-static)

# Build shaderc from source
if(SHADERC_FROM_SOURCE)
    # spirv-headers
    set(BUILD_TESTS OFF)
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON)
    FetchContent_Declare(
        SPIRV-Headers
        GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git"
        GIT_TAG "sdk-1.3.243.0"
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
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
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
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
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
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
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
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
endif()

# NFD
FetchContent_Declare(
    nfd
    GIT_REPOSITORY "https://github.com/btzy/nativefiledialog-extended.git"
    GIT_TAG "master"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(nfd)
target_link_libraries(${PROJECT_NAME} PUBLIC nfd)
get_target_property(NFD_DIR nfd SOURCE_DIR)
target_include_directories(${PROJECT_NAME} PUBLIC ${NFD_DIR}/include)
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
FetchContent_Declare(
    ryml
    GIT_REPOSITORY "https://github.com/biojppm/rapidyaml.git"
    GIT_TAG "master"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(ryml)
target_link_libraries(${PROJECT_NAME} PUBLIC ryml::ryml)
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
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(ktx)
target_link_libraries(${PROJECT_NAME} PUBLIC ktx)
set_targets_static_and_folder(ktx)
disable_ipo_target(ktx)

# FMT
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
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
    GIT_TAG "master"
    SOURCE_SUBDIR "Build"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JoltPhysics)
target_link_libraries(${PROJECT_NAME} PUBLIC Jolt)
target_include_directories(${PROJECT_NAME} PUBLIC ${JoltPhysics_SOURCE_DIR}/..)
FOLDER_DIR_TARGETS(Jolt)
