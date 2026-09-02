cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(src /usr/include/mono-2.0)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

add_library(Engine ${SOURCES})

target_link_libraries(Engine
        PUBLIC
        GLEW
        GL
        assimp
        fmt
        mono-2.0
        openal
        glfw
        jpeg
        png
        z
        physx
        nvtt
        perlin-noise
)
