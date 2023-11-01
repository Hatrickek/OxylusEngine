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

# Target name
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "OxylusEditor"
    TARGET_NAME_RELEASE "OxylusEditor"
    TARGET_NAME_Distribution    "OxylusEditor"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/ktx/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Oxylus/vendor/miniaudio"
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

# Copy resource folder
add_custom_command(TARGET OxylusEditor POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/OxylusEditor/Resources $<TARGET_FILE_DIR:OxylusEditor>/Resources)

# Set working directory
set(VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/OxylusEditor)

