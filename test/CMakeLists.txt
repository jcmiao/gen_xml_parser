cmake_minimum_required(VERSION 3.15.0)

include_directories(
    /home/jcmiao/workspace/mainline/dev/server/skeleton
    /home/jcmiao/workspace/mainline/dev/server/external/include/libxml2
    ${PROJECT_SOURCE_DIR}/test
    ${PROJECT_SOURCE_DIR}/build
)

link_directories(
    /home/jcmiao/workspace/mainline/dev/server/build/skeleton/moon
    /home/jcmiao/workspace/mainline/dev/server/external/lib
)

FILE(GLOB SRC "${PROJECT_SOURCE_DIR}/test/*.cpp")
FILE(GLOB GEN "${PROJECT_SOURCE_DIR}/build/*.cpp")

add_executable(test ${SRC} ${GEN})

target_link_libraries(test moon xml2 log4cxx pthread z)

add_dependencies(test gconf_tool)
