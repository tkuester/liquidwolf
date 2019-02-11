cmake_minimum_required(VERSION 3.0)
project(liquidwolf C)

find_library(SNDFILE
    NAMES sndfile
)

find_library(LIQUIDDSP
    NAMES liquid
)

execute_process(
  COMMAND git describe --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions("-D_LIQUIDWOLF_VERSION=\"${GIT_COMMIT_HASH}\"")

include_directories(include)
add_subdirectory(src)
enable_testing()
add_subdirectory(test)
