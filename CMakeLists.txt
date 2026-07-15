cmake_minimum_required(VERSION 3.20)

project(peNES LANGUAGES CXX)
find_package(SDL3 REQUIRED)

add_executable(peNES
	src/main.cpp
	src/cpu.cpp
	src/ines.cpp
	src/ppu.cpp
	src/mmu.cpp
)

target_link_libraries(peNES PRIVATE SDL3::SDL3)
set_target_properties(peNES PROPERTIES CXX_STANDARD 23)
target_compile_options(peNES PRIVATE -march=native)
