cmake_minimum_required(VERSION 3.22)
project(demo)

set(INCLUDE_DIR "/usr/local/include")  # 或实际安装路径
set(LIBRARY_DIR "/usr/local/lib")  # 或实际库路径

include_directories(${INCLUDE_DIR})
link_directories(${LIBRARY_DIR})

# 添加 /usr/local 到 CMake 的搜索路径
#list(APPEND CMAKE_PREFIX_PATH "/usr/local")
#list(APPEND CMAKE_LIBRARY_PATH "/usr/local/lib")
#list(APPEND CMAKE_INCLUDE_PATH "/usr/local/include")

#find_package(RABBTMQ_C REQUIRED)


add_executable(target
        source/rabbitmq-c.c
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
#target_link_directories(target )
#target_link_libraries(target )