cmake_minimum_required(VERSION 3.19)
project(asp-demo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG 3.3.2
)

FetchContent_MakeAvailable(glfw)

add_executable(asp-demo demo/main.cpp demo/glad/glad.c)

target_link_libraries(asp-demo glfw)
target_include_directories(asp-demo PUBLIC ./ demo)
target_compile_definitions(asp-demo PRIVATE -DGLFW_INCLUDE_NONE)