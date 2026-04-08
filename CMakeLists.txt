cmake_minimum_required(VERSION 3.14)
project(SKP2GLTF)

# 添加编译类型配置
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug/Release)" FORCE)
endif()

# 根据编译类型设置编译选项
if(MSVC)
    # 增加安全警告忽略
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    # Debug 配置使用 /MDd (Dynamic Debug)
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")
    # Release 配置使用 /MD (Dynamic Release)
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
endif()

set(CMAKE_CXX_STANDARD 17)

set(BUILD_USE_64BITS on)
# set(BUILD_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/windows/)

# 添加 CMake 模块路径
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
# 包含 SketchUp SDK 配置
include(sketchup-sdk)

# 自动从官方 GitHub 下载并集成 Draco
include(FetchContent)
FetchContent_Declare(
    draco
    GIT_REPOSITORY https://github.com/google/draco.git
    GIT_TAG        1.5.7  # 使用稳定的 1.5.7 版本
)

# 配置 Draco 编译选项，仅编译核心库以节省时间
set(DRACO_INSTALL OFF CACHE BOOL "" FORCE)
set(DRACO_JS_GLUE OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION ON CACHE BOOL "" FORCE)
set(DRACO_MESH_COMPRESSION ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(draco)

# 自动从官方 GitHub 下载并集成 meshoptimizer
FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        v0.21
)
FetchContent_MakeAvailable(meshoptimizer)


# 显式将 draco 的头文件路径添加到其接口中，以便链接它的目标能正确获取包含路径
target_include_directories(draco INTERFACE 
    ${draco_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# 添加子目录
add_subdirectory(src/gltflib)
add_subdirectory(src/skp)

# 添加可执行文件
add_executable(skp2gltf src/main.cpp)

target_include_directories(skp2gltf PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/skp
    ${PROJECT_SOURCE_DIR}/thirdparty
    ${PROJECT_SOURCE_DIR}/thirdparty/tinygltf
    ${PROJECT_SOURCE_DIR}/thirdparty/nlohmann
    ${PROJECT_SOURCE_DIR}/thirdparty/SketchUpAPI
    ${draco_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# 根据编译器类型设置链接器选项
if(MSVC)
    # MSVC 编译器不需要这些链接选项
    add_compile_options(/utf-8)  # 设置 MSVC 编译器使用 UTF-8 编码
    target_link_options(skp2gltf PRIVATE)
else()
    # 对于 GCC/Clang 编译器保留原有选项
    target_link_options(skp2gltf PRIVATE 
        "-Wl,--start-group" 
        "-Wl,--end-group"
    )
endif()

target_link_libraries(skp2gltf PRIVATE
    gltflib
    skp2native
    draco # 使用 FetchContent 生成的 target
    meshoptimizer
    ${CMAKE_SOURCE_DIR}/lib/SketchUpAPI.lib
    ${CMAKE_SOURCE_DIR}/lib/sketchup.lib
)

# 复制运行时依赖的 SKP_DLL 到输出目录
add_custom_command(TARGET skp2gltf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/SketchUpAPI.dll"
        "${CMAKE_SOURCE_DIR}/lib/SketchUpCommonPreferences.dll"
        "$<TARGET_FILE_DIR:skp2gltf>"
)

