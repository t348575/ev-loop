cmake_minimum_required(VERSION 3.5.1)

project(EvLoop CXX)

add_executable(Test test_ev_loop.cpp)

add_subdirectory(ev_loop)

target_link_directories(Test PUBLIC ev_loop/)

target_link_libraries(Test ev_loop pthread)