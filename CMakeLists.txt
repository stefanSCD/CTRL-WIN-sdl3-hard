cmake_minimum_required(VERSION 3.20)
project(VerticalScrollingShooter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL3_DIR "C:/libs/SDL3/cmake")
find_package(SDL3 REQUIRED CONFIG)

add_executable(VerticalScrollingShooter main.cpp rectangle.cpp circle.cpp)
target_include_directories(VerticalScrollingShooter PRIVATE "C:/dev/glm")
target_link_libraries(VerticalScrollingShooter PRIVATE SDL3::SDL3)

add_custom_command(TARGET VerticalScrollingShooter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:VerticalScrollingShooter>)

if(MINGW)
    target_link_options(VerticalScrollingShooter PRIVATE -static-libgcc -static-libstdc++)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET VerticalScrollingShooter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINGW_BIN_DIR}/libwinpthread-1.dll"
            $<TARGET_FILE_DIR:VerticalScrollingShooter>)
endif()
