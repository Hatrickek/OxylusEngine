set(PROJECT_NAME OxylusRuntime)

# Source groups
file(GLOB_RECURSE src "src/*.h" "src/*.cpp")
source_group("src" FILES ${src})
set(ALL_FILES ${src})

# Target
add_executable(${PROJECT_NAME} ${ALL_FILES})

set(ROOT_NAMESPACE OxylusRuntime)

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "OxylusRuntime"
    TARGET_NAME_RELEASE "OxylusRuntime"
    TARGET_NAME_Distribution    "OxylusRuntime"
)

# MSVC OxylusRuntime library

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
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/miniaudio"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/tracy/public"
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
)

# Compile and link options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE PRIVATE /std:c++20 /permissive-)
    add_definitions("/MP")
    target_link_options(${PROJECT_NAME} PRIVATE PRIVATE /DEBUG:FULL)
	set(CMAKE_SHARED_LINKER_FLAGS_DISTRIBUTION ${CMAKE_SHARED_LINKER_FLAGS})
    set(CMAKE_EXE_LINKER_FLAGS_DISTRIBUTION ${CMAKE_EXE_LINKER_FLAGS})
endif()

# Link with oxylus.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Oxylus
)
