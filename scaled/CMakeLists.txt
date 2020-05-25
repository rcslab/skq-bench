cmake_minimum_required(VERSION 3.10.0)
project(scaled)

set(CFLAGS -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -std=c11 -O2 -g)

add_executable(scaled scaled.c)
target_link_libraries(scaled pthread)
