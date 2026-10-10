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
    src/desktop_icons.cpp
    src/window_devour.cpp
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
    src/desktop_icons.h
    src/window_devour.h
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

# ---- 单元测试：潮汐时间模型 + D5 可见半径不变量（T8）----
# Why: 纯函数镜像，不依赖 OpenGL/GUI，可单独构建运行；与主 target 无耦合。
add_executable(tidal_model_test src/tidal_model_test.cpp)

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

# ---- 资源同步（F5）----
# Why: shader 由运行时 ReadFile 加载，不是编译依赖。仅改 .glsl 后 make 判定"无工作"，
#      build/shaders 留旧版 → 运行时加载过期 shader（本轮修复差点因此再次失效）。
#      自定义 target + DEPENDS 时间戳：shader 变化才重新拷贝；ALL 保证默认构建覆盖。
file(GLOB BH_SHADER_FILES ${CMAKE_SOURCE_DIR}/shaders/*.glsl)
add_custom_target(sync_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/texture
        ${CMAKE_BINARY_DIR}/texture
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/blackhole.glsl
        ${CMAKE_BINARY_DIR}/blackhole.glsl
    DEPENDS ${BH_SHADER_FILES}
    COMMENT "Syncing shaders and textures to build directory"
)
add_dependencies(blackhole sync_assets)

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
