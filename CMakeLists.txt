cmake_minimum_required(VERSION 3.9.4)

project(binspector)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")

file(GLOB SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/source/*.cpp)
file(GLOB_RECURSE BOOST_SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/boost/libs/*.cpp)
file(GLOB_RECURSE DBL_CONV_SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/double-conversion/src/*.cc)
file(GLOB_RECURSE ASL_SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/adobe_source_libraries/source/*.cpp)
set(APL_SRC_FILES ${PROJECT_SOURCE_DIR}/adobe_platform_libraries/source/expression_formatter.cpp)

# Remove some Boost files we know we're not using
file(GLOB_RECURSE BOOST_THREAD_BUILD_SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/boost/libs/thread/build/*.cpp)
list(REMOVE_ITEM BOOST_SRC_FILES ${BOOST_THREAD_BUILD_SRC_FILES})
list(REMOVE_ITEM BOOST_SRC_FILES ${PROJECT_SOURCE_DIR}/boost/libs/thread/src/pthread/once.cpp)

# Remove some ASL files we know we're not using
list(REMOVE_ITEM ASL_SRC_FILES ${PROJECT_SOURCE_DIR}/adobe_source_libraries/source/eve.cpp)

# Remove some windows-specific files from the MacOS build.
if (APPLE)
    file(GLOB BOOST_WIN_SRC_FILES_0 CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/boost/libs/thread/src/win32/*.cpp)

    list(REMOVE_ITEM BOOST_SRC_FILES ${BOOST_WIN_SRC_FILES_0})
    list(REMOVE_ITEM BOOST_SRC_FILES ${PROJECT_SOURCE_DIR}/boost/libs/program_options/src/winmain.cpp)
endif ()

add_executable(binspector
               ${SRC_FILES}
               ${BOOST_SRC_FILES}
               ${DBL_CONV_SRC_FILES}
               ${ASL_SRC_FILES}
               ${APL_SRC_FILES})

add_compile_definitions(ADOBE_STD_SERIALIZATION=1)

target_include_directories(binspector PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(binspector PUBLIC ${PROJECT_SOURCE_DIR}/adobe_platform_libraries)
target_include_directories(binspector PUBLIC ${PROJECT_SOURCE_DIR}/adobe_source_libraries)
target_include_directories(binspector PUBLIC ${PROJECT_SOURCE_DIR}/boost)
target_include_directories(binspector PUBLIC ${PROJECT_SOURCE_DIR}/stlab)

target_compile_options(binspector PUBLIC -Wall -Werror)
