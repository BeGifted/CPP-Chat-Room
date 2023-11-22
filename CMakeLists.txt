cmake_minimum_required(VERSION 3.0)
project(chatroom)

include (CPP-Linux-Server/cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++20 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)
include_directories(/usr/local/include)
include_directories(/usr/include)
link_directories(/usr/local/lib64)
link_directories(/usr/lib)


set(LIB_SRC
    chat/module.cc
)

add_library(chatroom SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(chatroom) #__FILE__


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)