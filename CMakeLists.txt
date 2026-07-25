cmake_minimum_required(VERSION 3.20)
include(FetchContent)

project(peNES LANGUAGES CXX)

option(PENES_ENABLE_ALL_WARNINGS "Show all warnings (except useless ones)" FALSE)
option(PENES_ENABLE_ASAN "Enable address santizier" FALSE)
option(PENES_ENABLE_MSAN "Enable memory santizier" FALSE)
option(PENES_MICROPROFILE "Enable microprofile" FALSE)

FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG "v0.9.8"
  GIT_SHALLOW 1
)
message("Downloading magic_enum...")
FetchContent_MakeAvailable(magic_enum)

add_library(peNES_opts INTERFACE)
target_link_libraries(peNES_opts INTERFACE magic_enum::magic_enum)

if(LINUX OR ANDROID)
  set(PENES_PLAT src/plat/plinux.cpp)
elseif(WIN32)
  set(PENES_PLAT src/plat/pwin.cpp)
else()
  message(FATAL_ERROR "Unsupported platfrom")
endif()

add_executable(peNES
  src/main.cpp
  src/cpu.cpp
  src/apu.cpp
  src/ppu.cpp
  src/ines.cpp
  src/cpuhooks.cpp
  ${PENES_PLAT}
  src/console.cpp
)
target_link_libraries(peNES PRIVATE peNES_opts)

add_subdirectory(src/mappers)
add_subdirectory(src/gui)

set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_LOCALE OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_DLOPEN_NOTES OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSHAPE OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
set(SDL_X11_XDBE OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  sdl3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG "release-3.4.12"
  GIT_SHALLOW 1
)
message("Downloading SDL3...")
FetchContent_MakeAvailable(sdl3)

target_link_libraries(peNES PRIVATE SDL3::SDL3-static)

target_compile_features(peNES_opts INTERFACE cxx_std_23)
target_compile_options(peNES_opts INTERFACE -march=native)
target_link_options(peNES_opts INTERFACE -march=native)
target_link_libraries(peNES_opts INTERFACE peNES_mappers)
target_link_libraries(peNES PRIVATE peNES_gui)

if(PENES_ENABLE_ASAN)
  target_compile_options(peNES_opts INTERFACE -O0 -fsanitize=address,undefined)
  target_link_options(peNES_opts INTERFACE -fsanitize=address,undefined)
endif()

if(PENES_ENABLE_MSAN)
  target_compile_options(peNES_opts INTERFACE -O0 -fsanitize=memory)
  target_link_options(peNES_opts INTERFACE -fsanitize=memory)
endif()

if(PENES_ENABLE_ASAN OR PENES_ENABLE_MSAN)
  target_compile_options(peNES_opts INTERFACE -fno-omit-frame-pointer)
  target_link_options(peNES_opts INTERFACE -fno-omit-frame-pointer)
endif()

if(PENES_ENABLE_ALL_WARNINGS)
  target_compile_options(peNES_opts INTERFACE -Wall -Wextra -Wshadow -Wpedantic -Wformat=2 -Wuninitialized -Wno-c99-extensions -Wno-gnu-anonymous-struct)
endif()

if(PENES_MICROPROFILE)
  FetchContent_Declare(
    microprofile
    GIT_REPOSITORY https://github.com/jonasmr/microprofile.git
    GIT_TAG "v4.0"
    GIT_SHALLOW 1
  )
  message("Downloading microprofile...")
  FetchContent_MakeAvailable(microprofile)

  target_link_libraries(peNES_opts INTERFACE microprofile)
  target_compile_definitions(peNES_opts INTERFACE PENES_MICROPROFILE)
endif()
