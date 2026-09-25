cmake_minimum_required(VERSION 3.20)
project(valigo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(EMSCRIPTEN)
  # Tell raylib to build its web (WebGL + Emscripten GLFW) backend.
  set(PLATFORM Web CACHE STRING "raylib platform" FORCE)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(valigo src/main.cpp)
target_link_libraries(valigo PRIVATE raylib)

if(MSVC)
  target_compile_options(valigo PRIVATE /W4)
else()
  target_compile_options(valigo PRIVATE -Wall -Wextra)
endif()

if(EMSCRIPTEN)
  set_target_properties(valigo PROPERTIES SUFFIX ".html")
  target_link_options(valigo PRIVATE
    -sUSE_GLFW=3
    -sGL_ENABLE_GET_PROC_ADDRESS=1
    -sALLOW_MEMORY_GROWTH=1
    # miniaudio's Web Audio backend reads Module.HEAPF32, which newer
    # Emscripten versions no longer export by default.
    -sEXPORTED_RUNTIME_METHODS=HEAPF32
    --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
  )
  set_property(TARGET valigo APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
endif()
