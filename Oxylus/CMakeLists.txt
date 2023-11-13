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
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW/include"
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
  add_definitions("/MP")
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
source_group("vendor/ImGui" FILES ${ImGuiFiles})

set(ImGui_Backends
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.h"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.h"
)
target_sources(${PROJECT_NAME} PUBLIC "${ImGui_Backends}")
source_group("vendor/ImGui" FILES ${ImGui_Backends})

set(ImGui_Misc "${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp" "${IMGUI_DIR}/misc/cpp/imgui_stdlib.h")
target_sources(${PROJECT_NAME} PUBLIC "${ImGui_Misc}")
source_group("vendor/ImGui" FILES ${ImGui_Misc})

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

# tinygltf
set(TINYGLTF_NO_INCLUDE_JSON ON)
set(TINYGLTF_USE_RAPIDJSON ON)
set(TINYGLTF_USE_CPP14 ON)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)
set(TINYGLTF_HEADER_ONLY ON)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY "https://github.com/syoyo/tinygltf"
    GIT_TAG "release"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(tinygltf)
target_link_libraries(${PROJECT_NAME} PUBLIC tinygltf)
set_targets_static_and_folder(tinygltf)

# FMT
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG "10.0.0"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(fmt)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt)
set_targets_static_and_folder(fmt)

# spdlog
set(SPDLOG_FMT_EXTERNAL TRUE)
add_definitions(-DSPDLOG_LEVEL_NAMES={\"TRACE\",\"DEBUG\",\"INFO\",\"WARN\",\"ERROR\",\"CRITICAL\",\"OFF\"})
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG "v1.x"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
set_targets_static_and_folder(spdlog)

# vk-bootstrap
FetchContent_Declare(
    vk-bootstrap 
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        8e61b2d81c3f5f84339735085ff5651f71bbe1e7
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PUBLIC vk-bootstrap)
set_targets_static_and_folder(vk-bootstrap)

# vuk
set(VUK_LINK_TO_LOADER OFF)
set(VUK_FAIL_FAST ON)
set(VUK_USE_DXC ON)
FetchContent_Declare(
    vuk
    GIT_REPOSITORY "https://github.com/martty/vuk.git"
    GIT_TAG "dev"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(vuk)
target_link_libraries(${PROJECT_NAME} PUBLIC vuk)
set_targets_static_and_folder(vuk)

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

# RapidJSON
FetchContent_Declare(
    RapidJSON
    GIT_REPOSITORY https://github.com/Tencent/rapidjson
    GIT_TAG "master"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_Populate(RapidJSON)
set(RapidJSON_DIR "${FETCHCONTENT_BASE_DIR}/rapidjson-src")
target_include_directories(${PROJECT_NAME} PUBLIC ${RapidJSON_DIR}/include)

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

# tracy
set(TRACY_STATIC ON)
set(TRACY_VK_USE_SYMBOL_TABLE ON)
FetchContent_Declare(
    Tracy
    GIT_REPOSITORY "https://github.com/wolfpld/tracy.git"
    GIT_TAG v0.10
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(Tracy)
target_link_libraries(${PROJECT_NAME} PUBLIC Tracy::TracyClient)
set_targets_static_and_folder(TracyClient)

# lua
set(LUA_ENABLE_SHARED OFF)
set(LUA_ENABLE_TESTING OFF)
FetchContent_Declare(
    lua
    GIT_REPOSITORY "https://github.com/walterschell/Lua"
    GIT_TAG master
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(lua)
target_link_libraries(${PROJECT_NAME} PUBLIC lua_static)
set_targets_static_and_folder(lua_static)

# sol
set(SOL2_LUA_VERSION 5.4.6)
FetchContent_Declare(
    sol
    GIT_REPOSITORY "https://github.com/ThePhD/sol2"
    GIT_TAG v3.3.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(sol)
target_link_libraries(${PROJECT_NAME} PUBLIC sol2)
set_targets_static_and_folder(sol2)