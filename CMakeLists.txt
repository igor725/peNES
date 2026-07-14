cmake_minimum_required(VERSION 3.16)
project(peNES LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
find_package(SDL3 REQUIRED)

add_compile_options(-march=native)

add_executable(peNES
	src/main.cpp
	src/cpu.cpp
	src/ines.cpp
	src/ppu.cpp
	src/mmu.cpp
)
target_link_libraries(peNES PRIVATE SDL3::SDL3)
