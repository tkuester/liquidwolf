cmake_minimum_required(VERSION 3.13)
project(liquidwolf C)

execute_process(
  COMMAND git describe --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_DESCRIBE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

find_library(SNDFILE
    NAMES sndfile
)

find_library(LIQUIDDSP
    NAMES liquid
)

include_directories(include)
add_subdirectory(src)
enable_testing()
add_subdirectory(test)
