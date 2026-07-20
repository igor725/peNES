add_library(peNES_mappers STATIC
  base.cpp
  mmc0.cpp
  mmc1.cpp
  cnrom.cpp
)
target_link_libraries(peNES_mappers PRIVATE peNES_opts)
