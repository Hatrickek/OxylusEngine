set(PROJECT_NAME Oxylus)

file(GLOB_RECURSE src "src/*.h" "src/*.cpp")
source_group("src" FILES ${src})

set(vendor__ImGui
  "vendor/ImGui/imconfig.h"
  "vendor/ImGui/imgui.cpp"
  "vendor/ImGui/imgui.h"
  "vendor/ImGui/imgui_demo.cpp"
  "vendor/ImGui/imgui_draw.cpp"
  "vendor/ImGui/imgui_internal.h"
  "vendor/ImGui/imgui_tables.cpp"
  "vendor/ImGui/imgui_widgets.cpp"
  "vendor/ImGui/imstb_rectpack.h"
  "vendor/ImGui/imstb_textedit.h"
  "vendor/ImGui/imstb_truetype.h"
  )
source_group("vendor/ImGui" FILES ${vendor__ImGui})

set(vendor__ImGui__backends
  "vendor/ImGui/backends/imgui_impl_glfw.cpp"
  "vendor/ImGui/backends/imgui_impl_glfw.h"
  "vendor/ImGui/backends/imgui_impl_vulkan.cpp"
  "vendor/ImGui/backends/imgui_impl_vulkan.h"
  )
source_group("vendor/ImGui/backends" FILES ${vendor__ImGui__backends})

set(vendor__ImGui__misc__cpp
  "vendor/ImGui/misc/cpp/imgui_stdlib.cpp"
  "vendor/ImGui/misc/cpp/imgui_stdlib.h"
  )
source_group("vendor/ImGui/misc/cpp" FILES ${vendor__ImGui__misc__cpp})

set(vendor__ImGuizmo
  "vendor/ImGuizmo/ImGuizmo.cpp"
  "vendor/ImGuizmo/ImGuizmo.h"
  )
source_group("vendor/ImGuizmo" FILES ${vendor__ImGuizmo})

file(GLOB vendor__glm "vendor/glm/*.hpp")
source_group("vendor/glm" FILES ${vendor__glm})

set(vendor__miniaudio
  "vendor/miniaudio/miniaudio.h"
  )
source_group("vendor/miniaudio" FILES ${vendor__miniaudio})

set(vendor__tracy__public
  "vendor/tracy/public/TracyClient.cpp"
  )
source_group("vendor/tracy/public" FILES ${vendor__tracy__public})

set(ALL_FILES
  ${src}
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
  TARGET_NAME_DIST "Oxylus"
  TARGET_NAME_RELEASE "Oxylus"
  )
# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
  OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Debug-windows-x86_64/Oxylus/"
  OUTPUT_DIRECTORY_DIST "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/Dist-windows-x86_64/Oxylus/"
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
  "TRACY_ENABLE"
  ">"
  "$<$<CONFIG:Dist>:"
  "OX_DIST;"
  "NDEBUG"
  ">"
  "$<$<CONFIG:Release>:"
  "OX_RELEASE;"
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
    $<$<CONFIG:Dist>:
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
    $<$<CONFIG:Dist>:
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
  $<$<CONFIG:Dist>:
  MultiThreaded
  >
  $<$<CONFIG:Release>:
  MultiThreaded
  >
  $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Dist>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
  )
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

#-------------
# Dependencies
#-------------

include(FetchContent)

#build libraries as static
set(BUILD_SHARED_LIBS OFF)

# Vulkan
find_package(Vulkan)

# Extract the Vulkan SDK path
if (NOT ${Vulkan_INCLUDE_DIRS} STREQUAL "")
  set(VULKAN_PATH ${Vulkan_INCLUDE_DIRS})
  STRING(REGEX REPLACE "/Include" "" VULKAN_PATH ${VULKAN_PATH})
endif()

# Include Vulkan header files 
include_directories(${Vulkan_INCLUDE_DIRS})

if (WIN32)
  set(Vulkan_shaderc_combined_LIBRARY "${VULKAN_PATH}/lib/shaderc_shared.lib")
elseif (APPLE)
  set(Vulkan_shaderc_combined_LIBRARY "${VULKAN_PATH}/../lib/libshaderc_shared.dylib")
endif()

set(VULKAN_LIB_LIST "${Vulkan_LIBRARIES}" "${Vulkan_shaderc_combined_LIBRARY}")
target_link_libraries(${PROJECT_NAME} PUBLIC ${VULKAN_LIB_LIST} )

# NFD
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/nativefiledialog-extended)
target_link_libraries(${PROJECT_NAME} PUBLIC nfd)
set_target_properties(nfd PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
set_target_properties(glfw PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

# RYML
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/ryml)
target_link_libraries(${PROJECT_NAME} PUBLIC ryml::ryml)
set_target_properties(ryml PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
target_link_libraries(${PROJECT_NAME} PUBLIC c4core)
set_target_properties(c4core PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

# KTX
set(KTX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/KTX-Software-4.1.0-rc3")
if (NOT EXISTS ${KTX_DIR})
  file(DOWNLOAD https://github.com/KhronosGroup/KTX-Software/archive/refs/tags/v4.1.0-rc3.zip ktx.zip SHOW_PROGRESS)
  file(ARCHIVE_EXTRACT INPUT ktx.zip DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
endif()

set(KTX_FEATURE_TESTS OFF)
set(KTX_FEATURE_GL_UPLOAD OFF)
set(KTX_FEATURE_STATIC_LIBRARY ON)
add_subdirectory(${KTX_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ktx)
target_include_directories(${PROJECT_NAME} PUBLIC ${KTX_DIR}/include)
set_target_properties(ktx PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

# FMT
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG master)
FetchContent_MakeAvailable(fmt)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt)

