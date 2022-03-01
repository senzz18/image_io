cmake_minimum_required(VERSION 3.13)

project(image_io LANGUAGES CXX DESCRIPTION "image_io test")
enable_language(CXX)

#### Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2 -mcpu=apple-m1 -mtune=native")
        if (NOT MINGW)
                set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        endif()
        set(CMAKE_CXX_FLAGS_RelWithDebInfo "${CMAKE_CXX_FLAGS} -O2 -g")
        add_compile_options (-fdiagnostics-color=always)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2 -mcpu=apple-m1 -mtune=native")
        if (NOT MINGW)
                set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        endif()
        set(CMAKE_CXX_FLAGS_RelWithDebInfo "${CMAKE_CXX_FLAGS} -O2 -g")
        add_compile_options (-fcolor-diagnostics)
endif()

add_executable(image_io_test main.cpp image_io.cpp pgm_io.cpp pgx_io.cpp)
set_target_properties(image_io_test PROPERTIES OUTPUT_NAME $<IF:$<CONFIG:Debug>,image_io_test_dbg,image_io_test>)