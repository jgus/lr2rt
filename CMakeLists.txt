cmake_minimum_required(VERSION 3.14)
project(lr2rt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ${CMAKE_CXX_STANDARD})

find_program(CONAN_BIN NAMES conan)
if(NOT CONAN_BIN)
    message(FATAL_ERROR "Could not find conan, which is required for this build. Please install it (e.g. `pip install conan`)")
endif()
execute_process(COMMAND "${CONAN_BIN}" install "${CMAKE_SOURCE_DIR}" --build=missing -s build_type=${CMAKE_BUILD_TYPE})
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(lr2rt "")
target_link_libraries(lr2rt PRIVATE
        CONAN_PKG::boost
        CONAN_PKG::Exiv2
        )
target_sources(lr2rt PRIVATE
        main.cc
        )
