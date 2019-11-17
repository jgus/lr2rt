cmake_minimum_required(VERSION 3.14)
project(lr2rt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ${CMAKE_CXX_STANDARD})

find_program(CONAN_BIN NAMES conan)
if (NOT CONAN_BIN)
    message(FATAL_ERROR "Could not find conan, which is required for this build. Please install it (e.g. `pip install conan`)")
endif ()
execute_process(COMMAND "${CONAN_BIN}" install "${CMAKE_SOURCE_DIR}" --build=missing -s build_type=Release)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# The Conan-generated targets for Exiv2 are messed up for all build configs except release, so mimic that here
add_library(Exiv2 INTERFACE)
target_link_libraries(Exiv2 INTERFACE
        ${CONAN_PACKAGE_TARGETS_EXIV2} ${CONAN_SHARED_LINKER_FLAGS_EXIV2_LIST} ${CONAN_EXE_LINKER_FLAGS_EXIV2_LIST}
        ${CONAN_PACKAGE_TARGETS_EXIV2_RELEASE} ${CONAN_SHARED_LINKER_FLAGS_EXIV2_RELEASE_LIST} ${CONAN_EXE_LINKER_FLAGS_EXIV2_RELEASE_LIST}
        )
target_include_directories(Exiv2 INTERFACE
        ${CONAN_INCLUDE_DIRS_EXIV2} ${CONAN_INCLUDE_DIRS_EXIV2_RELEASE}
        )
target_compile_definitions(Exiv2 INTERFACE
        ${CONAN_COMPILE_DEFINITIONS_EXIV2} ${CONAN_COMPILE_DEFINITIONS_EXIV2_RELEASE}
        )
target_compile_options(Exiv2 INTERFACE
        ${CONAN_C_FLAGS_EXIV2_LIST} ${CONAN_CXX_FLAGS_EXIV2_LIST} ${CONAN_C_FLAGS_EXIV2_RELEASE_LIST} ${CONAN_CXX_FLAGS_EXIV2_RELEASE_LIST}
        -Wno-deprecated-declarations
        )

add_executable(lr2rt "")
target_link_libraries(lr2rt PRIVATE
        CONAN_PKG::Boost
        Exiv2
        )
target_sources(lr2rt PRIVATE
        import_crop.cc
        import_development.cc
        import_tags.cc
        lr2rt_main.cc
        metadata.cc
        settings.cc
        )

add_executable(match_dev "")
target_link_libraries(match_dev PRIVATE
        CONAN_PKG::Boost
        CONAN_PKG::libjpeg-turbo
        CONAN_PKG::TIFF
        CONAN_PKG::libpng
        )
target_sources(match_dev PRIVATE
        match_dev_main.cc
        settings.cc
        )
