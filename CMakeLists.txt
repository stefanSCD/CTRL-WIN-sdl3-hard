
cmake_minimum_required(VERSION 3.20)

project(SDLHelloWorld)

set(CMAKE_CXX_STANDARD 17)

# SDL3
set(SDL3_DIR "C:/libs/SDL3/cmake")
find_package(SDL3 REQUIRED CONFIG)

# OpenGL
find_package(OpenGL REQUIRED)

add_executable(SDLHelloWorld main.cpp)

target_link_libraries(SDLHelloWorld
        PRIVATE
        SDL3::SDL3
        OpenGL::GL
)

# Copy SDL3.dll
if(TARGET SDL3::SDL3-shared)
    add_custom_command(
            TARGET SDLHelloWorld POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:SDLHelloWorld>
            VERBATIM
    )
endif()
