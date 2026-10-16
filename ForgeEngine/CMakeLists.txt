cmake_minimum_required(VERSION 3.21)

project(ForgeEngine
    VERSION 0.5.11
    DESCRIPTION "Modern C++17 DirectX 12 3D engine, C# scripting API and editor"
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "ForgeEngine requires Windows, MSVC and the Windows SDK.")
endif()

# Dear ImGui and ImGuizmo are fetched from their upstream repositories on the
# first configure. Git and an internet connection are required only for this step.
# Reuse populated FetchContent directories between incremental builds to avoid
# repeatedly downloading or reconfiguring large third-party dependencies.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# CMake 3.21 compatibility: ImGui and ImGuizmo are source-only repositories,
# so populate them without adding them as standalone CMake projects.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(dear_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_GetProperties(dear_imgui)
if(NOT dear_imgui_POPULATED)
    FetchContent_Populate(dear_imgui)
endif()

FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

# Assimp provides production-grade import support for OBJ, FBX, glTF/GLB, DAE, STL, etc.
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(assimp)

set(IMGUI_ROOT "${dear_imgui_SOURCE_DIR}")
set(IMGUI_BACKENDS "${IMGUI_ROOT}/backends")

add_library(imgui STATIC
    "${IMGUI_ROOT}/imgui.cpp"
    "${IMGUI_ROOT}/imgui_draw.cpp"
    "${IMGUI_ROOT}/imgui_tables.cpp"
    "${IMGUI_ROOT}/imgui_widgets.cpp"
    "${IMGUI_BACKENDS}/imgui_impl_win32.cpp"
    "${IMGUI_BACKENDS}/imgui_impl_dx12.cpp"
)
target_include_directories(imgui PUBLIC
    "${IMGUI_ROOT}"
    "${IMGUI_BACKENDS}"
)
target_compile_features(imgui PUBLIC cxx_std_17)
target_compile_definitions(imgui PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(imgui PUBLIC d3d12 dxgi imm32 dwmapi)

add_library(ImGuizmo STATIC
    "${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp"
)
target_include_directories(ImGuizmo PUBLIC
    "${imguizmo_SOURCE_DIR}/src"
    "${IMGUI_ROOT}"
)
target_compile_features(ImGuizmo PUBLIC cxx_std_17)
target_compile_definitions(ImGuizmo PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(ImGuizmo PUBLIC imgui)

add_executable(ForgeEngine WIN32
    src/App/main.cpp
    src/Engine/Engine.cpp
    src/Engine/Engine.h
    src/Editor/EditorUI.cpp
    src/Editor/EditorUI.h
    src/Scripting/ScriptRuntime.cpp
    src/Scripting/ScriptRuntime.h
    assets/shaders/Shaders.hlsl
)

set_source_files_properties(assets/shaders/Shaders.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)

target_compile_features(ForgeEngine PRIVATE cxx_std_17)
target_include_directories(ForgeEngine PRIVATE
    src/Engine
    src/Editor
    src/Scripting
)
target_compile_definitions(ForgeEngine PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(ForgeEngine PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /sdl
    )
endif()

target_link_libraries(ForgeEngine PRIVATE
    imgui
    ImGuizmo
    assimp::assimp
    windowscodecs
    ole32
    d3d12
    dxgi
    d3dcompiler
    dxguid
    comdlg32
    shell32
    user32
    gdi32
    imm32
    dwmapi
    advapi32
)

add_custom_command(TARGET ForgeEngine POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/Shaders.hlsl"
        "$<TARGET_FILE_DIR:ForgeEngine>/Shaders.hlsl"
    COMMENT "Copying runtime HLSL shader"
    VERBATIM
)

set_property(TARGET ForgeEngine PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ForgeEngine>"
)
