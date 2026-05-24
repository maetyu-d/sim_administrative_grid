cmake_minimum_required(VERSION 3.20)
project(NeonCivic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    FetchContent_Declare(
        SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

add_executable(neon_civic
    src/main.cpp
)

if(APPLE)
    target_compile_definitions(neon_civic PRIVATE GL_SILENCE_DEPRECATION)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(neon_civic PRIVATE SDL2::SDL2)
elseif(TARGET SDL2)
    target_link_libraries(neon_civic PRIVATE SDL2)
else()
    target_include_directories(neon_civic PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(neon_civic PRIVATE ${SDL2_LIBRARIES})
endif()

if(APPLE)
    target_link_libraries(neon_civic PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(neon_civic PRIVATE OpenGL::GL)
endif()
