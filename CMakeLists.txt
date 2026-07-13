cmake_minimum_required(VERSION 3.16)
project(NES LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
include(FetchContent)

add_compile_options(-march=native)

add_executable(nestest
	src/main.cpp
	src/cpu.cpp
	src/ines.cpp
	src/ppu.cpp
)
