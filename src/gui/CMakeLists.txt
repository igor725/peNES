FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG "v1.92.9"
  GIT_SHALLOW 1
)
message("Downloading imgui...")
FetchContent_MakeAvailable(imgui)

add_library(peNES_gui STATIC
  base.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(peNES_gui PRIVATE
  ${imgui_SOURCE_DIR}/
  ${imgui_SOURCE_DIR}/backends/
)
target_link_libraries(peNES_gui PRIVATE peNES_opts)
