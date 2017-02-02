cmake_minimum_required(VERSION 3.6)
project(C3)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c)
find_package( Threads REQUIRED )
add_executable(C3 ${SOURCE_FILES})
target_link_libraries(C3 Threads::Threads )

