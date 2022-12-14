cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(src ${CMAKE_SOURCE_DIR}/lib/inc)
link_directories(${CMAKE_SOURCE_DIR}/lib/bin)

add_compile_definitions(GLEW_STATIC)

add_library(Engine src/Pine/Pine.hpp src/Pine/Pine.hpp src/Pine/Engine/Engine.hpp src/Pine/Engine/Engine.cpp src/Pine/Core/WindowManager/WindowManager.hpp src/Pine/Core/Math/Math.hpp src/Pine/Core/Math/Vector2/Vector2.hpp src/Pine/Core/WindowManager/WindowManager.cpp src/Pine/Graphics/Interfaces/IGraphicsAPI.hpp src/Pine/Core/Color/Color.hpp src/Pine/Graphics/OpenGL/OpenGL.hpp src/Pine/Graphics/OpenGL/OpenGL.cpp src/Pine/Graphics/Graphics.hpp src/Pine/Graphics/Graphics.cpp src/Pine/Core/Log/Log.hpp src/Pine/Core/Log/Log.cpp ../Sandbox/src/Application.cpp)

target_link_libraries(Engine glfw3 glew32 opengl32)