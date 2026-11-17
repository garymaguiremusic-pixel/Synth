cmake_minimum_required(VERSION 3.10)
project(osirusRender)
add_executable(osirusRender osirusRender.cpp)
target_link_libraries(osirusRender PUBLIC virusLib)
