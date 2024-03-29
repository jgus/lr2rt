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

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_compile_definitions(TARGET_OS_IS_APPLE=1)
endif ()

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

add_library(Boost INTERFACE)
if (TARGET CONAN_PKG::Boost)
    target_link_libraries(Boost INTERFACE CONAN_PKG::Boost)
elseif (TARGET CONAN_PKG::boost)
    target_link_libraries(Boost INTERFACE CONAN_PKG::boost)
endif ()

add_library(CImg INTERFACE)
target_include_directories(CImg INTERFACE external/CImg)
target_compile_definitions(CImg INTERFACE cimg_display=0)
if (TARGET CONAN_PKG::libjpeg-turbo)
    target_compile_definitions(CImg INTERFACE cimg_use_jpeg=1)
    target_link_libraries(CImg INTERFACE CONAN_PKG::libjpeg-turbo)
endif ()
if (TARGET CONAN_PKG::TIFF)
    target_compile_definitions(CImg INTERFACE cimg_use_tiff=1)
    target_link_libraries(CImg INTERFACE CONAN_PKG::TIFF)
endif ()
if (TARGET CONAN_PKG::libpng)
    target_compile_definitions(CImg INTERFACE cimg_use_png=1)
    target_link_libraries(CImg INTERFACE CONAN_PKG::libpng)
endif ()

add_executable(lr2rt "")
target_link_libraries(lr2rt PRIVATE
        Boost
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
        Boost
        CImg
        )
target_sources(match_dev PRIVATE
        match_dev_main.cc
        render.cc
        settings.cc
        )
