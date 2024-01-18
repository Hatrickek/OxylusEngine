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
file(GLOB_RECURSE Scripting "src/Scripting/*.h" "src/Scripting/*.cpp" )
source_group("src/Scripting" FILES ${Scripting})

file(GLOB Render "src/Render/*.h" "src/Render/*.cpp" )
source_group("src/Render" FILES ${Render})
file(GLOB PBR "src/Render/PBR/*.h" "src/Render/PBR/*.cpp" )
source_group("src/Render/PBR" FILES ${PBR})
file(GLOB GTAO "src/Render/PBR/GTAO/*.h" "src/Render/PBR/GTAO/*.cpp" )
source_group("src/Render/PBR/GTAO" FILES ${GTAO})
file(GLOB Vulkan "src/Render/Vulkan/*.h" "src/Render/Vulkan/*.cpp" )
source_group("src/Render/Vulkan" FILES ${Vulkan})
file(GLOB VulkanUtils "src/Render/Vulkan/Utils/*.h" "src/Render/Vulkan/Utils/*.cpp" )
source_group("src/Render/Vulkan/Utils" FILES ${VulkanUtils})

set(RENDERER_FILES ${Render} ${PBR} ${GTAO} ${Vulkan} ${VulkanUtils})

file(GLOB_RECURSE Scene "src/Scene/*.h" "src/Scene/*.cpp" )
source_group("src/Scene" FILES ${Scene})
file(GLOB_RECURSE Thread "src/Thread/*.h" "src/Thread/*.cpp" )
source_group("src/Thread" FILES ${Thread})
file(GLOB_RECURSE UI "src/UI/*.h" "src/UI/*.cpp" )
source_group("src/UI" FILES ${UI})
file(GLOB_RECURSE Utils "src/Utils/*.h" "src/Utils/*.cpp" )
source_group("src/Utils" FILES ${Utils})

set(SOURCE_FILES ${src} ${Core} ${Assets} ${Audio} ${Event} ${Physics} 
                  ${RENDERER_FILES} ${Scene} ${Thread} ${UI} ${Utils} ${Scripting})

set(vendor__ImGuizmo "vendor/ImGuizmo/ImGuizmo.cpp" "vendor/ImGuizmo/ImGuizmo.h")
source_group("vendor/ImGuizmo" FILES ${vendor__ImGuizmo})

set(vendor__miniaudio "vendor/miniaudio/miniaudio.h")
source_group("vendor/miniaudio" FILES ${vendor__miniaudio})

set(ALL_FILES
  ${SOURCE_FILES}
  ${vendor__ImGuizmo}
  ${vendor__miniaudio}
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
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio"
  )

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
  "$<$<CONFIG:Debug>:"
  "OX_DEBUG;"
  "_DEBUG;"
  ">"
  "$<$<CONFIG:Release>:"
  "OX_RELEASE;"
  "NDEBUG;"
  ">"
  "$<$<CONFIG:Distribution>:"
  "OX_DISTRIBUTION;"
  "NDEBUG"
  ">"
  "_CRT_SECURE_NO_WARNINGS"
  "GLFW_INCLUDE_NONE"
  "_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS"
  "GLM_FORCE_DEPTH_ZERO_TO_ONE"
  )

#-------------
# MSVC Specific
#-------------

# Compile and link options
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /std:c++20 /permissive-) 
  set(CMAKE_SHARED_LINKER_FLAGS_DISTRIBUTION ${CMAKE_SHARED_LINKER_FLAGS})
  set(CMAKE_EXE_LINKER_FLAGS_DISTRIBUTION ${CMAKE_EXE_LINKER_FLAGS})
endif ()

if (MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_definitions("/MP")
endif()

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
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/CPM.cmake)
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1) # hack for removing auto generated ctest targets

# Suppress all warnings from dependencies.
if(MSVC)
    set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /W0)
else()
    set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS -w)
endif()

set(CMAKE_C_COMPILER_AR "${CMAKE_AR}")
set(CMAKE_CXX_COMPILER_AR "${CMAKE_AR}")
set(CMAKE_C_COMPILER_RANLIB "${CMAKE_RANLIB}")
set(CMAKE_CXX_COMPILER_RANLIB "${CMAKE_RANLIB}")

include(FetchContent)

# Vulkan
if (VULKAN_SDK)
    message("Within Oxylus CMake, setting VULKAN_SDK env var to " ${VULKAN_SDK})
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

# dear imgui
CPMAddPackage("gh:ocornut/imgui#docking")
target_include_directories(${PROJECT_NAME} PUBLIC ${imgui_SOURCE_DIR})

file(GLOB ImGuiFiles "${imgui_SOURCE_DIR}/*.h" "${imgui_SOURCE_DIR}/*.cpp")
target_sources(${PROJECT_NAME} PUBLIC "${ImGuiFiles}")
source_group("vendor/ImGui" FILES ${ImGuiFiles})

set(ImGui_Backends
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h"
)
target_sources(${PROJECT_NAME} PUBLIC "${ImGui_Backends}")
source_group("vendor/ImGui" FILES ${ImGui_Backends})

set(ImGui_Misc "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp" "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h")
target_sources(${PROJECT_NAME} PUBLIC "${ImGui_Misc}")
source_group("vendor/ImGui" FILES ${ImGui_Misc})

# entt
CPMAddPackage(
  NAME EnTT
  VERSION 3.12.2
  GITHUB_REPOSITORY skypjack/entt
  DOWNLOAD_ONLY YES
)
if (EnTT_ADDED)
    add_library(EnTT INTERFACE)
    target_include_directories(EnTT INTERFACE ${EnTT_SOURCE_DIR}/src)
    target_link_libraries(${PROJECT_NAME} PUBLIC EnTT)
endif()

# tinygltf
CPMAddPackage(
    NAME tinygltf
    GIT_TAG release
    GITHUB_REPOSITORY syoyo/tinygltf
    OPTIONS
        "TINYGLTF_NO_INCLUDE_JSON ON"
        "TINYGLTF_USE_RAPIDJSON ON"
        "TINYGLTF_USE_CPP14 ON"
        "TINYGLTF_BUILD_LOADER_EXAMPLE OFF"
        "TINYGLTF_HEADER_ONLY ON"
)
set_targets_static_and_folder(tinygltf)
target_link_libraries(${PROJECT_NAME} PUBLIC tinygltf)
# FMT
CPMAddPackage("gh:fmtlib/fmt#10.1.1")
target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)
set_target_properties(fmt PROPERTIES FOLDER "Vendor")

# fmtlog
CPMAddPackage(
    NAME fmtlog
    GITHUB_REPOSITORY Hatrickek/fmtlog
    GIT_TAG main
    GIT_SUBMODULES ""
    DOWNLOAD_ONLY YES
)
if (fmtlog_ADDED)
    FILE(GLOB fmtlog_sources ${fmtlog_SOURCE_DIR}/*.cc)
    add_library(fmtlog STATIC ${fmtlog_sources})
    target_include_directories(fmtlog PUBLIC ${fmtlog_SOURCE_DIR})
    target_include_directories(fmtlog PUBLIC ${fmt_SOURCE_DIR}/include)
    target_include_directories(${PROJECT_NAME} PUBLIC ${fmtlog_SOURCE_DIR})
    target_link_libraries(${PROJECT_NAME} PUBLIC fmtlog)
endif()
set_targets_static(fmtlog)
set_target_properties(fmtlog PROPERTIES FOLDER "Vendor/fmtlog")

# vk-bootstrap
CPMAddPackage("gh:charles-lunarg/vk-bootstrap#8e61b2d81c3f5f84339735085ff5651f71bbe1e7")
target_link_libraries(${PROJECT_NAME} PUBLIC vk-bootstrap)
set_targets_static_and_folder(vk-bootstrap)

# vuk
CPMAddPackage(
    NAME vuk
    GITHUB_REPOSITORY martty/vuk
    GIT_TAG master
    OPTIONS
        "VUK_LINK_TO_LOADER OFF"
        "VUK_FAIL_FAST ON"
        "VUK_USE_DXC ON"
)
target_link_libraries(${PROJECT_NAME} PUBLIC vuk)
set_targets_static_and_folder(vuk)

# NFD
CPMAddPackage("gh:btzy/nativefiledialog-extended#master")
target_link_libraries(${PROJECT_NAME} PUBLIC nfd)
target_include_directories(${PROJECT_NAME} PUBLIC ${NFD_SOURCE_DIR}/src/include)
set_targets_static_and_folder(nfd)

# GLFW
CPMAddPackage(
    NAME GLFW
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.8
    OPTIONS
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BULID_DOCS OFF"
	"GLFW_INSTALL OFF"
)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
set_targets_static_and_folder(glfw)

# RYML
CPMAddPackage("gh:biojppm/rapidyaml#master")
target_link_libraries(${PROJECT_NAME} PUBLIC ryml)
target_link_libraries(${PROJECT_NAME} PUBLIC c4core)
set_targets_static_and_folder(ryml)

# RapidJSON
CPMAddPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG master
    DOWNLOAD_ONLY YES
)
target_include_directories(${PROJECT_NAME} PUBLIC ${rapidjson_SOURCE_DIR}/include)

# Jolt
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    GIT_TAG master
    SOURCE_SUBDIR Build
    OPTIONS
        "COMPILE_AS_SHARED_LIBRARY OFF"
        "ENABLE_ALL_WARNINGS OFF"
        "USE_STATIC_MSVC_RUNTIME_LIBRARY ON"
        "INTERPROCEDURAL_OPTIMIZATION OFF"
)
target_link_libraries(${PROJECT_NAME} PUBLIC Jolt)
target_include_directories(${PROJECT_NAME} PUBLIC ${JoltPhysics_SOURCE_DIR}/..)
set_targets_static_and_folder(Jolt)

# tracy
CPMAddPackage(
    NAME TracyClient
    GITHUB_REPOSITORY wolfpld/tracy
    VERSION 0.10
    OPTIONS
        "TRACY_ENABLE OFF"
        "TRACY_STATIC ON"
        "TRACY_VK_USE_SYMBOL_TABLE ON"
        "TRACY_NO_FRAME_IMAGE ON"
        "TRACY_ON_DEMAND OFF"
)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
set_targets_static_and_folder(TracyClient)

CPMAddPackage(
    NAME lua
    GITHUB_REPOSITORY walterschell/Lua
    GIT_TAG 88246d621abf7b6fba9332f49229d507f020e450
    OPTIONS
        "LUA_SUPPORT_DL OFF"
        "LUA_BUILD_AS_CXX OFF"
        "LUA_ENABLE_SHARED OFF"
        "LUA_ENABLE_TESTING OFF"
        "LUA_BUILD_COMPILER OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${lua_SOURCE_DIR}/lua-5.4.6/include)

CPMAddPackage(
    NAME sol2
    GITHUB_REPOSITORY ThePhD/sol2
    VERSION 3.3.0
    OPTIONS
        "SOL2_BUILD_LUA OFF"
        "SOL2_TESTS OFF"
        "SOL2_TESTS_SINGLE OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${sol2_SOURCE_DIR}/include)
target_link_libraries(sol2 INTERFACE $<BUILD_INTERFACE:${lua_static}>)
target_link_libraries(${PROJECT_NAME} PUBLIC sol2 lua_static)
set_targets_static(lua_static)
set_target_properties(lua_static PROPERTIES FOLDER "Vendor")

# enkiTS
CPMAddPackage(
    NAME enkiTS
    GITHUB_REPOSITORY dougbinks/enkiTS
    GIT_TAG master
    OPTIONS
        "ENKITS_BUILD_C_INTERFACE OFF"
        "ENKITS_BUILD_EXAMPLES OFF"
)
target_include_directories(${PROJECT_NAME} PUBLIC ${enkiTS_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PUBLIC enkiTS)
set_target_properties(enkiTS PROPERTIES FOLDER "Vendor")
set_targets_static(enkiTS)
