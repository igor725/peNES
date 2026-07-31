add_library(peNES_mappers STATIC
  base.cpp
  nrom.cpp
  mmc1.cpp
  mmc3.cpp
  cnrom.cpp
  unrom.cpp
)
target_link_libraries(peNES_mappers PRIVATE peNES_opts)
