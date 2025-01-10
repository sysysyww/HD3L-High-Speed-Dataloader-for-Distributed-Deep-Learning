cmake_minimum_required(VERSION 3.10) # 根据你的CMake版本调整
project(MyDistributedProject VERSION 1.0)

# 设置C++标准为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# 包含头文件目录
include_directories(src/common) # 假设common目录下的文件对所有部分都是可见的
include_directories(src/client) # 包含client头文件目录
include_directories(src/server) # 包含server头文件目录

# 定义构建选项
option(BUILD_CLIENT "Build the client application" ON)
option(BUILD_SERVER "Build the server application" OFF)

# 公共源文件
set(COMMON_SOURCES
    src/common/utils.cpp
    src/common/logging.cpp
)

# 条件编译：客户端
if(BUILD_CLIENT)
    # 添加客户端可执行文件
    add_executable(client
        src/client/client.cpp
        src/client/client_data_handler.cpp
        ${COMMON_SOURCES}
        src/client/BatchDataProcessor.cpp
    )
    # 链接客户端所需的库
    target_link_libraries(client pthread rt)
    # 添加静态链接libstdc++的标志
    target_link_options(client PRIVATE -static-libstdc++)
    # 添加AddressSanitizer支持
    # target_compile_options(client PRIVATE -fsanitize=address)
    # target_link_options(client PRIVATE -fsanitize=address)
endif()

# 条件编译：服务器
if(BUILD_SERVER)
    # 添加服务器可执行文件
    find_package(TBB REQUIRED)
    find_package(Boost REQUIRED COMPONENTS system thread)
    add_executable(server
        src/server/server.cpp
        src/server/server_data_handler.cpp
        src/server/CurrentController.cpp
        src/server/flow_control.cpp
        ${COMMON_SOURCES}
    )
    # 链接服务器所需的库
    target_link_libraries(server pthread TBB::tbb Boost::system Boost::thread)
    # 添加AddressSanitizer支持
    # if(TBB_FOUND AND Boost_FOUND)
        # target_compile_options(server PRIVATE -fsanitize=address)
        # target_link_options(server PRIVATE -fsanitize=address)
    #endif()
endif()
