cmake_minimum_required(VERSION 3.15)
project(MiciusFCEmulator VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 版本
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 使用 FetchContent 下载 SDL2
include(FetchContent)
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9 # 使用 SDL2 的最新版本
)
FetchContent_MakeAvailable(SDL2)

# 收集所有源文件
file(GLOB_RECURSE SOURCES src/*.cpp)

# 添加可执行文件
add_executable(MiciusFCEmulator ${SOURCES})

# 设置包含目录（所有的头文件在 src 中）
target_include_directories(MiciusFCEmulator PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 链接 SDL2 库
#target_link_libraries(MiciusFCEmulator PRIVATE SDL2::SDL2)
target_link_libraries(MiciusFCEmulator PRIVATE SDL2::SDL2 SDL2::SDL2main)


# 添加调试宏定义
target_compile_definitions(MiciusFCEmulator PRIVATE DEBUG_MODE=1)

# 在构建之后将 SDL2d.dll 复制到可执行文件所在目录
add_custom_command(TARGET MiciusFCEmulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:MiciusFCEmulator>)