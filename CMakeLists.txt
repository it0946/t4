cmake_minimum_required(VERSION 3.22)
project(t4 C)

set(C_STANDARD 11)

add_executable(t4 src/main.c src/stset.c src/mem.c src/rtinfo.c)
target_include_directories(t4 PRIVATE include)
target_compile_options(t4 PRIVATE -mavx2 -mbmi)

# set_property(TARGET t4 PROPERTY C_STANDARD 11)