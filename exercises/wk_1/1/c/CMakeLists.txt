cmake_minimum_required(VERSION 3.6)
project(c)



set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        ex2.c)

find_package( Threads REQUIRED )
add_executable(c ${SOURCE_FILES})
target_link_libraries(c Threads::Threads )