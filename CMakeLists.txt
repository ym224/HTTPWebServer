cmake_minimum_required(VERSION 3.8)
project(starter_code)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        echo_client.c
        lisod.c
        log.c)

add_executable(starter_code ${SOURCE_FILES})