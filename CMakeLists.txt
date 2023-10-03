cmake_minimum_required(VERSION 3.10)
project(uelfld)
add_executable(uelfld main.cpp elf_loader.cpp stack_setup.cpp)
set(CMAKE_BUILD_TYPE Debug)