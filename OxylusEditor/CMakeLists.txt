set(PROJECT_NAME OxylusEditor)

# Source groups
file(GLOB_RECURSE src "src/*.h" "src/*.cpp")
source_group("src" FILES ${src})

set(ALL_FILES
    ${src}
)

# Target
add_executable(${PROJECT_NAME} ${ALL_FILES})

set(ROOT_NAMESPACE OxylusEditor)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "OxylusEditor"
    TARGET_NAME_DIST    "OxylusEditor"
    TARGET_NAME_RELEASE "OxylusEditor"
)

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/../bin/Debug-windows-x86_64/OxylusEditor/"
    OUTPUT_DIRECTORY_DIST    "${CMAKE_CURRENT_SOURCE_DIR}/../bin/Dist-windows-x86_64/OxylusEditor/"
    OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../bin/Release-windows-x86_64/OxylusEditor/"
)

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

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/GLFW/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/ImGui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/entt"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/ImGuizmo"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/tinygltf"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/ktx/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/miniaudio"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/tracy/public"
    "$ENV{VULKAN_SDK}/include"
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
    "_HAS_EXCEPTIONS=0;"
    "UNICODE;"
    "_UNICODE"
)

# Compile and link options
if(MSVC)
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
        /W3;
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /DEBUG;
            /INCREMENTAL
        >
        $<$<CONFIG:Dist>:
            /OPT:REF;
            /OPT:ICF;
        >
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF;
        >
        /SUBSYSTEM:CONSOLE
    )
endif()

# Link with oxylus.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Oxylus
)
