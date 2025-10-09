cmake_minimum_required(VERSION 3.22)

#详细构建过程
set(CMAKE_VERBOSE_MAKEFILE ON)

# 告知当前使用的是交叉编译方式，必须配置
#SET(CMAKE_SYSTEM_NAME Linux)
#set(CMAKE_BUILD_TYPE Debug)

#设置C11标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

#设置工具链
#SET(CMAKE_C_COMPILER "/opt/develop/rootfs/buildroot-2021.11.1/output/host/bin/arm-linux-gcc")
#SET(CMAKE_CXX_COMPILER "/opt/develop/rootfs/buildroot-2021.11.1/output/host/bin/arm-linux-g++")

#指定库文件和头文件的搜索目录
set(INCLUDE_DIR
    #ubuntu
    "/usr/local/include"
    #debian
#    "/root/rabbitmq-c-0.15.0/build/lib/include/"
)  # 或实际头文件路径
set(LIBRARY_DIR
    #ubuntu
    "/usr/lib"
    "/usr/local/lib"
    #debian
#    "/root/rabbitmq-c-0.15.0/build/lib/lib/"
)  # 或实际库路径
include_directories(${INCLUDE_DIR})
link_directories(${LIBRARY_DIR})

# 添加 /usr/local 到 CMake 的搜索路径
#list(APPEND CMAKE_PREFIX_PATH "/usr/local")
#list(APPEND CMAKE_LIBRARY_PATH "/usr/local/lib")
#list(APPEND CMAKE_INCLUDE_PATH "/usr/local/include")

#find_package(RABBTMQ_C REQUIRED)


# 源码目录
aux_source_directory(./source PROJ_SRC)
aux_source_directory(./demo PROJ_DEMO_SRC)

add_executable(target
#        ./source/main.c
        ${PROJ_SRC}
#        ${PROJ_DEMO_SRC}
)

message(STATUS "==============================")
# 输出系统包含路径
message(STATUS "CMAKE_INCLUDE_CURRENT_DIR: ${CMAKE_INCLUDE_PATH}")
message(STATUS "CMAKE_LIBRARY_PATH: ${CMAKE_LIBRARY_PATH}")
message(STATUS "CMAKE_SYSTEM_INCLUDE_PATH: ${CMAKE_SYSTEM_INCLUDE_PATH}")
message(STATUS "CMAKE_SYSTEM_LIBRARY_PATH: ${CMAKE_SYSTEM_LIBRARY_PATH}")

# 输出当前查找路径
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")


message(STATUS "==============================")
target_link_libraries(target rabbitmq)


project(demo)