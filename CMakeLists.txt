cmake_minimum_required(VERSION 3.22)
project(vfs C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

set(EXECUTABLE_NAME vfs)
set(EXECUTABLE_NAME_TOOLS vfstools)

file(GLOB_RECURSE EXECUTABLE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/vfs.c"
)

file(GLOB_RECURSE EXECUTABLE_SOURCES_TOOLS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/vfstool.c"
)

add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES})
add_executable(${EXECUTABLE_NAME_TOOLS} ${EXECUTABLE_SOURCES_TOOLS})

# manual build paths
set(LIBFUSE_DIR ${CMAKE_CURRENT_LIST_DIR}/third_party/libfuse)
set(LIBFUSE_BUILD_DIR ${LIBFUSE_DIR}/build)

# include dirs
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${LIBFUSE_DIR}/include
    ${LIBFUSE_DIR}/lib
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# link to static libfuse3
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    ${LIBFUSE_BUILD_DIR}/lib/libfuse3.a
    pthread
    dl
)

target_compile_options(${EXECUTABLE_NAME} PRIVATE
    -Wall
    -Wextra
    -Wconversion
    -Wstrict-prototypes
)

target_compile_options(${EXECUTABLE_NAME_TOOLS} PRIVATE
    -Wall
    -Wextra
    -Wconversion
    -Wstrict-prototypes
)
