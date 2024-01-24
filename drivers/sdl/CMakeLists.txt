add_library(sdl INTERFACE)

target_sources(sdl INTERFACE ${CMAKE_CURRENT_LIST_DIR}/sdl.c)

target_include_directories(sdl INTERFACE ${CMAKE_CURRENT_LIST_DIR})

