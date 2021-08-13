cmake_minimum_required(VERSION 3.15.0)

FILE(GLOB SRC "*.cpp")

add_executable(gconf_tool ${SRC})

target_link_libraries(gconf_tool clang)