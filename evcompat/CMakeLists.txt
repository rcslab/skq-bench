cmake_minimum_required(VERSION 3.10)

project(evcompat)

find_program(cc "clang")

set(CMAKE_C_COMPILER ${cc})

if(BUILD STREQUAL "DEBUG")
set(CFLAGS -g -Wall -Werror -Og -std=c11 -DDEBUG)
else()
set(CFLAGS -g -Wall -Werror -O2 -std=c11 -DNDEBUG)
endif()

add_library(event STATIC event.c)
target_compile_options(event PRIVATE ${CFLAGS})

add_executable(test test.c)
target_link_libraries(test event pthread)
target_compile_options(test PRIVATE ${CFLAGS})
