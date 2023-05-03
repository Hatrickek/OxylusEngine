set(PROJECT_NAME OxylusEditor)

# Source groups
file(GLOB src "src/*.h" "src/*.cpp")
source_group("src" FILES ${src})
file(GLOB_RECURSE Panels "src/Panels/*.h" "src/Panels/*.cpp" )
source_group("src/Panels" FILES ${Panels})
file(GLOB_RECURSE Utils "src/Utils/*.h" "src/Utils/*.cpp" )
source_group("src/Utils" FILES ${Utils})

set(ALL_FILES ${src} ${Panels} ${Utils})

# Target
add_executable(${PROJECT_NAME} ${ALL_FILES})

set(ROOT_NAMESPACE OxylusEditor)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "OxylusEditor"
    TARGET_NAME_RELEASE "OxylusEditor"
    TARGET_NAME_Distribution    "OxylusEditor"
)

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/../bin/Debug-windows-x86_64/OxylusEditor/"
    OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../bin/Release-windows-x86_64/OxylusEditor/"
    OUTPUT_DIRECTORY_Distribution    "${CMAKE_CURRENT_SOURCE_DIR}/../bin/Distribution-windows-x86_64/OxylusEditor/"
)

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
  $<CONFIG:Release>,
  $<CONFIG:Distribution>
  >>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
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
)

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "OX_DEBUG;"
        "_DEBUG;"
        "TRACY_ENABLE"
    ">"
    "$<$<CONFIG:Release>:"
        "OX_RELEASE;"
        "NDEBUG;"
        "TRACY_ENABLE"
    ">"
    "$<$<CONFIG:Distribution>:"
        "OX_Distribution;"
        "NDEBUG"
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
        $<$<CONFIG:Release>:
            /Ox;
            /Oi;
            /GF;
            /Gy;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT}
        >
        $<$<CONFIG:Distribution>:
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
        $<$<CONFIG:Release>:
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
endif()

# Link with oxylus.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Oxylus
)
