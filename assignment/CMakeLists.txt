cmake_minimum_required(VERSION 3.6)
project(assignment)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES tmp.c)
add_executable(assignment ${SOURCE_FILES})
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(assignment Threads::Threads)