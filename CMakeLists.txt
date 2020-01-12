cmake_minimum_required(VERSION 3.15)
project(ProgParralProject C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all")

#include_directories(src)

add_executable(ProgParralProject
        src/bitmap.c
        src/bitmap.h
        src/edge-detect.c)