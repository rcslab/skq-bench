cmake_minimum_required(VERSION 3.10)

project(evcompat)

find_program(cc "clang")
set(CMAKE_C_COMPILER ${cc})

set(CFLAGS_DBG -g -Wall -Wextra -Werror -Og -march=native -std=c11 -DDEBUG)
set(CFLAGS -g -Wall -Wextra -Werror -O2 -march=native -std=c11 -DNDEBUG)

# Release build
add_library(event STATIC event.c)
target_compile_options(event PRIVATE ${CFLAGS})

# debug build
add_library(event_dbg STATIC event.c)
target_compile_options(event_dbg PRIVATE ${CFLAGS_DBG})

# test
add_executable(test test.c)
target_link_libraries(test event_dbg pthread)
target_compile_options(test PRIVATE ${CFLAGS_DBG})
