cmake_minimum_required(VERSION 3.8)
project(project_1)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        echo_client.c
        example.c
        lex.yy.c
        lisod.c
        log.c
        log.h
        parse.c
        parse.h
        process_request.c
        process_request.h
        y.tab.c
        y.tab.h)

add_executable(project_1 ${SOURCE_FILES})