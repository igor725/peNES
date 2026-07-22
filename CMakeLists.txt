cmake_minimum_required(VERSION 3.20)

project(peNES LANGUAGES CXX)

option(PENES_ENABLE_ALL_WARNINGS "Show all warnings (except useless ones)" FALSE)
option(PENES_ENABLE_HEADLESS "Headless mode, no graphics or SDL whatsoever (useful for CPU debugging)" FALSE)
option(PENES_ENABLE_ASAN "Enable address santizier" FALSE)
option(PENES_ENABLE_MSAN "Enable memory santizier" FALSE)

add_library(peNES_opts INTERFACE)

if(LINUX)
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
  src/mmu.cpp
  src/ines.cpp
  src/cpuhooks.cpp
  ${PENES_PLAT}
)
target_link_libraries(peNES PRIVATE peNES_opts)

add_subdirectory(src/mappers)

if(PENES_ENABLE_HEADLESS)
  target_compile_definitions(peNES_opts PRIVATE PENES_NO_SDL)
else()
  find_package(SDL3 REQUIRED)
  target_link_libraries(peNES PRIVATE SDL3::SDL3)
endif()

target_compile_features(peNES_opts INTERFACE cxx_std_23)
target_compile_options(peNES_opts INTERFACE -march=native)
target_link_options(peNES_opts INTERFACE -march=native)
target_link_libraries(peNES_opts INTERFACE peNES_mappers)

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
