cmake_minimum_required(VERSION 3.16)
set(ENV{BTSTACK_ROOT} "${CMAKE_CURRENT_SOURCE_DIR}/components/btstack")
set(CONFIG_BTSTACK_MESH_ENABLED OFF CACHE BOOL "" FORCE)

# Ограничиваем NAPT таблицу для экономии памяти
# add_compile_definitions(IP_NAPT_MAX=64)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(satura-bridge)