cmake_minimum_required(VERSION 3.15)
project(MiciusFCEmulator VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 版本
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用 FetchContent 下载 SDL3
include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://gitee.com/changey/SDL.git
    GIT_TAG release-3.2.10 # 使用 SDL3 的最新版本
)
FetchContent_MakeAvailable(SDL3)

# 收集所有源文件
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

message(STATUS "Collected sources: ${SOURCES}")

# 添加可执行文件（去掉 WIN32）
add_executable(MiciusFCEmulator ${SOURCES})

# 设置包含目录（所有的头文件在 src 中）
target_include_directories(MiciusFCEmulator PRIVATE "${CMAKE_SOURCE_DIR}/src")

# 链接 SDL3 库
target_link_libraries(MiciusFCEmulator PRIVATE SDL3::SDL3)

# 区分 Debug 和 Release 宏定义
target_compile_definitions(MiciusFCEmulator PRIVATE 
    $<$<CONFIG:Debug>:DEBUG_MODE=1>
    $<$<CONFIG:Release>:NDEBUG>
)

# 区分 Debug 和 Release 编译选项（可选）
if(MSVC)
    target_compile_options(MiciusFCEmulator PRIVATE 
        $<$<CONFIG:Debug>:/W4 /Zi>
        $<$<CONFIG:Release>:/O2>
    )
else()
    target_compile_options(MiciusFCEmulator PRIVATE 
        $<$<CONFIG:Debug>:-g -Wall -Wextra>
        $<$<CONFIG:Release>:-O3 -Wall -Wextra>
    )
endif()

# 在构建之后将 SDL3.dll 复制到可执行文件所在目录
add_custom_command(TARGET MiciusFCEmulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:MiciusFCEmulator>)