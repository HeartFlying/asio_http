#
#    asio_http: http client library for boost asio
#    Copyright (c) 2017 Julio Becerra Gomez 
#    See COPYING for license information.
#

cmake_minimum_required(VERSION 3.4)
project(asio_http)

option(BUILD_ASIO_HTTP_TESTS "build tests and examples")

if((${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang") AND NOT(CMAKE_CXX_COMPILER_VERSION LESS 5.0))
  set(HAS_CORO 1)
endif()

if(HAS_CORO)
 set(CORO_FLAGS "-fcoroutines-ts -stdlib=libc++")
else()
 set(CORO_FLAGS "")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CORO_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

INCLUDE_DIRECTORIES(/opt/local/include)
LINK_DIRECTORIES(/opt/local/lib)
find_package(Boost COMPONENTS system REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads)

set(INTERFACE_FILES
  interface/asio_http/coro_handler.h
  interface/asio_http/error.h
  interface/asio_http/future_handler.h
  interface/asio_http/http_request.h
  interface/asio_http/http_request_result.h
  interface/asio_http/http_client.h
  interface/asio_http/http_client_settings.h
  interface/asio_http/http_request_interface.h
  interface/asio_http/url.h
)

if(NOT HAS_CORO)
    list(REMOVE_ITEM INTERFACE_FILES interface/asio_http/coro_handler.h)
endif()

set(IMPLEMENTATION_SOURCES
  implementation/completion_handler_invoker.cpp
  implementation/connection_pool.cpp
  implementation/data_sink.cpp
  implementation/data_source.cpp
  implementation/http_client.cpp
  implementation/request_manager.cpp
  implementation/logging_functions.cpp
  implementation/compression.cpp
  implementation/http_request.cpp
  implementation/url.cpp
)

set(IMPLEMENTATION_HEADERS
  implementation/interface/asio_http/internal/completion_handler_invoker.h
  implementation/interface/asio_http/internal/http_client_connection.h
  implementation/interface/asio_http/internal/connection_pool.h
  implementation/interface/asio_http/internal/data_sink.h
  implementation/interface/asio_http/internal/data_source.h
  implementation/interface/asio_http/internal/request_manager.h
  implementation/interface/asio_http/internal/logging_functions.h
  implementation/interface/asio_http/internal/request_data.h
  implementation/interface/asio_http/internal/compression.h
  implementation/interface/asio_http/internal/socket.h
)
add_library(${PROJECT_NAME} STATIC
  ${INTERFACE_FILES}
  ${IMPLEMENTATION_SOURCES}
  ${IMPLEMENTATION_HEADERS}
)

#target_compile_features(asio_http PRIVATE cxx_std_14)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${Boost_LIBRARIES}
    http_parser # public for errors enum
  PRIVATE
    loguru
    ${ZLIB_LIBRARIES}
    ${CMAKE_DL_LIBS} # For loguru dladdr
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    implementation/interface
    interface
)

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This adds
# the following targets: gtest, gtest_main, gmock
# and gmock_main
add_subdirectory("${CMAKE_BINARY_DIR}/googletest-src"
                 "${CMAKE_BINARY_DIR}/googletest-build")

enable_testing()
if(BUILD_ASIO_HTTP_TESTS)
  add_subdirectory(test)
  add_subdirectory(test_server)
  add_subdirectory(examples)
endif()
add_subdirectory(loguru)
add_subdirectory(http_parser)
