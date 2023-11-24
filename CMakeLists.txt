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

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

set(LIB_SRC
    chatroom/application.cc
    chatroom/chatServlet.cc
    chatroom/protocol.cc
    chatroom/resServlet.cc
)

add_library(chatroom SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(chatroom) #__FILE__

set(LIB_LIB
    chatroom
    chat
    dl
    jsoncpp
    pthread
    -L/usr/local/lib -lyaml-cpp
    ${OPENSSL_LIBRARIES}
)

add_executable(main chatroom/main.cc)
add_dependencies(main chatroom)
force_redefine_file_macro_for_sources(main) #__FILE__
target_link_libraries(main ${LIB_LIB})


SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)