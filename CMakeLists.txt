cmake_minimum_required(VERSION 3.20)

project(peNES LANGUAGES CXX)

option(PENES_ENABLE_ALL_WARNINGS "Show all warnings (except useless ones)" FALSE)
option(PENES_ENABLE_HEADLESS "Headless mode, no graphics or SDL whatsoever (useful for CPU debugging)" FALSE)
option(PENES_ENABLE_ASAN "Enable address santizier" FALSE)
option(PENES_ENABLE_MSAN "Enable memory santizier" FALSE)

add_executable(peNES
	src/main.cpp
	src/cpu.cpp
	src/apu.cpp
	src/ppu.cpp
	src/mmu.cpp
	src/ines.cpp
	src/cpuhooks.cpp
	src/mappers/mmc0.cpp
	src/mappers/mmc1.cpp
)

if(PENES_ENABLE_HEADLESS)
	target_compile_definitions(peNES PRIVATE PENES_NO_SDL)
else()
	find_package(SDL3 REQUIRED)
	target_link_libraries(peNES PRIVATE SDL3::SDL3)
endif()

set_target_properties(peNES PROPERTIES CXX_STANDARD 23)
target_compile_options(peNES PRIVATE -march=native)
target_link_options(peNES PRIVATE -march=native)

if(PENES_ENABLE_ASAN)
	target_compile_options(peNES PRIVATE -O0 -fsanitize=address,undefined)
	target_link_options(peNES PRIVATE -fsanitize=address,undefined)
endif()

if(PENES_ENABLE_MSAN)
	target_compile_options(peNES PRIVATE -O0 -fsanitize=memory)
	target_link_options(peNES PRIVATE -fsanitize=memory)
endif()

if(PENES_ENABLE_ASAN OR PENES_ENABLE_MSAN)
	target_compile_options(peNES PRIVATE -fno-omit-frame-pointer)
	target_link_options(peNES PRIVATE -fno-omit-frame-pointer)
endif()

if(PENES_ENABLE_ALL_WARNINGS)
	target_compile_options(peNES PRIVATE -Wall -Wextra -Wshadow -Wpedantic -Wformat=2 -Wuninitialized -Wno-c99-extensions -Wno-gnu-anonymous-struct)
endif()
