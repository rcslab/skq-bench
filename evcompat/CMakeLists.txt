cmake_minimum_required(VERSION 3.10)

project(evcompat)

find_program(cc "clang")
set(CMAKE_C_COMPILER ${cc})

set(CFLAGS_DBG -g -Wall -Wextra -Werror -Og -std=c11 -DDEBUG)
set(CFLAGS -g -Wall -Wextra -Werror -O2 -std=c11 -DNDEBUG)

# Release build
add_library(event STATIC event.c)
target_compile_options(event PRIVATE ${CFLAGS})

add_executable(test test.c)
target_link_libraries(test event pthread)
target_compile_options(test PRIVATE ${CFLAGS})

# debug build
add_library(event_dbg STATIC event.c)
target_compile_options(event_dbg PRIVATE ${CFLAGS_DBG})

add_executable(test_dbg test.c)
target_link_libraries(test_dbg event_dbg pthread)
target_compile_options(test_dbg PRIVATE ${CFLAGS_DBG})

