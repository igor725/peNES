FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG "v1.92.9"
  GIT_SHALLOW 1
)
message("Downloading imgui...")
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
  imgui_hex_editor
  GIT_REPOSITORY https://github.com/Teselka/imgui_hex_editor.git
  GIT_TAG "89eb70139b302e77ff138af401ae5f692df77513"
  GIT_SHALLOW 1
)
message("Downloading imgui_hex_editor...")
FetchContent_MakeAvailable(imgui_hex_editor)

add_library(peNES_gui STATIC
  gbase.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  ${imgui_hex_editor_SOURCE_DIR}/imgui_hex.cpp
)
target_include_directories(peNES_gui PRIVATE
  ${imgui_SOURCE_DIR}/
  ${imgui_SOURCE_DIR}/backends/
  ${imgui_hex_editor_SOURCE_DIR}/
)
target_link_libraries(peNES_gui PRIVATE peNES_opts)
