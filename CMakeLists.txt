cmake_minimum_required(VERSION 3.7)
project(HPNetServer)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        RR-Mode/config_file.h
        RR-Mode/defines.h
        RR-Mode/global_settings.h
        RR-Mode/init_configure.cc
        RR-Mode/init_configure.h
        RR-Mode/master_thread.cc
        RR-Mode/master_thread.h
        RR-Mode/RRMode.cpp
        RR-Mode/socket_wrapper.h
        RR-Mode/threadSafe_container.h
        RR-Mode/utils.h
        RR-Mode/worker_threads.cc
        RR-Mode/worker_threads.h)

add_executable(HPNetServer ${SOURCE_FILES})