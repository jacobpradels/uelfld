cmake_minimum_required(VERSION 3.10)
project(uelfld)
add_executable(uelfld main.cpp)
target_compile_options(uelfld PRIVATE -fpermissive)