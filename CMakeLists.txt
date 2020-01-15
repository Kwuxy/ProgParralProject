cmake_minimum_required(VERSION 3.14)
project(ProgParralProject C)

set(CMAKE_CXX_STANDARD 14)
set(GCC_COVERAGE_COMPILE_FLAGS "-O2 -ftree-vectorize -fopt-info -mavx2 -fopt-info-vec-all")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )

include_directories(./src)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(ProgParralProject
        src/bitmap.c
        src/edge-detect.c)

target_link_libraries(ProgParralProject Threads::Threads)
