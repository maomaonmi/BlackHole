cmake_minimum_required(VERSION 3.20)
project(BlackHole LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- blackhole.exe ----
set(BH_SOURCES
    src/main.cpp
    src/win32_gl.cpp
    src/gl_texture.cpp
    src/monitors.cpp
    src/capture_wgc.cpp
    src/gui_config.cpp
    src/star_texture.cpp
)

set(BH_HEADERS
    src/win32_gl.h
    src/gl_texture.h
    src/monitors.h
    src/capture_wgc.h
    src/texture_source.h
    src/renderer_interface.h
    src/gui_config.h
    src/star_texture.h
)

# ImGui sources
set(IMGUI_SOURCES
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_impl_win32.cpp
    src/imgui/imgui_impl_opengl3.cpp
)

add_executable(blackhole
    ${BH_SOURCES}
    ${BH_HEADERS}
    ${IMGUI_SOURCES}
    resource.rc
)

target_include_directories(blackhole PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/imgui
    ${CMAKE_SOURCE_DIR}/stb-master
)

target_compile_options(blackhole PRIVATE
    -fexec-charset=UTF-8
    -Wall -Wno-unknown-pragmas
)

target_compile_definitions(blackhole PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
)

target_link_libraries(blackhole PRIVATE
    opengl32
    gdi32
    dwmapi
    ole32
    oleaut32
    user32
    kernel32
    d3d11
    dxgi
    dxguid
    runtimeobject
    mfplat
    mfuuid
    strmiids
    wtsapi32
)

# Copy shaders to build directory
add_custom_command(TARGET blackhole POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/texture
        ${CMAKE_BINARY_DIR}/texture
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/blackhole.glsl
        ${CMAKE_BINARY_DIR}/blackhole.glsl
    COMMENT "Copying shaders and textures to build directory"
)

# ---- D3D11 renderer (optional, disabled by default) ----
option(BLACKHOLE_USE_D3D11 "Enable D3D11 rendering path" OFF)
if(BLACKHOLE_USE_D3D11)
    target_compile_definitions(blackhole PRIVATE BLACKHOLE_USE_D3D11)
    target_sources(blackhole PRIVATE
        src/d3d11_renderer.cpp
        src/capture_dxgi.cpp
        src/win32_window.cpp
    )
endif()
