cmake_minimum_required (VERSION 3.5)
set(PROJECT_NAME DaaS)
project (${PROJECT_NAME} VERSION 0.4.0)
# set (${PROJECT_NAME}_VERSION_MAJOR 0)
# set (${PROJECT_NAME}_VERSION_MINOR 3)

include_directories(${CMAKE_SOURCE_DIR})

include(ExternalProject)
# We need to include the source of templated classes as .tpp
add_executable(${PROJECT_NAME} DaaS.cpp
    shared_queue.hpp
    shared_queue.tpp

    net_api.hpp
    net_api.cpp

    # lightweight_logger.hpp
    logentries.hpp
    logentries.tpp
    logging.hpp

    datadog.hpp
    datadog.tpp
    metrics.hpp

    http_request_handler.hpp
    http_request_handler.tpp

    # inotify_monitor.hpp
)

# Clang does not support C++17 yet, we cannot use -std=c++17
# Boost uses exception
# set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -std=c++17 -Wpedantic -DBOOST_NO_EXCEPTIONS -fno-exceptions")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -std=c++1z -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -g  -DDEBUG_LEVEL=2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DDEBUG_LEVEL=0 -DNDEBUG")

# JSON CPP
# target_link_libraries (${PROJECT_NAME}  jsoncpp)

# PostgreSQL
target_link_libraries(${PROJECT_NAME} pqxx)

# target_link_libraries (${PROJECT_NAME} pthread mpi mpi_cxx ssl crypto rt)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries (${PROJECT_NAME}  ${CMAKE_THREAD_LIBS_INIT})

find_package(MPI REQUIRED)
target_link_libraries(${PROJECT_NAME} ${MPI_LIBRARIES})

# find_package(Boost 1.58.0)
find_package(Boost REQUIRED COMPONENTS thread system)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIRS})
target_link_libraries (${PROJECT_NAME} ${OPENSSL_LIBRARIES})


# Instead of using the development version,
# we might be able to use the system libraries
# when it includes libnghttp2_asio.so
#[[
include_directories (/usr/include/nghttp2)
target_link_libraries (${PROJECT_NAME} nghttp2_asio nghttp2_asio)
]]
# libnghttp2_asio requires:
# libboost-dev >= 1.54.0
# libboost-thread-dev >= 1.54.0
ExternalProject_Add(nghttp2
        PREFIX		nghttp2
        GIT_REPOSITORY	https://github.com/nghttp2/nghttp2
        #        GIT_TAG		v1.18.x
        #        GIT_TAG		v1.19.x
        GIT_TAG		master
        STEP_TARGETS	build
        EXCLUDE_FROM_ALL TRUE
        CMAKE_ARGS	-DENABLE_ASIO_LIB=yes
)
# add_dependencies(${PROJECT_NAME} nghttp2)
# set(NGHTTP2_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/include)
# set(NGHTTP2_LIBRARIES ${PROJECT_BINARY_DIR}/lib64/libnghttp2_asio.so)
add_dependencies(${PROJECT_NAME} nghttp2-build)
set(NGHTTP2_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/nghttp2/src/nghttp2/src/includes)
set(NGHTTP2_LIBRARIES    ${PROJECT_BINARY_DIR}/nghttp2/src/nghttp2-build/src/libnghttp2_asio.so
                         ${PROJECT_BINARY_DIR}/nghttp2/src/nghttp2-build/lib/libnghttp2.so  # For nghttp2_http2_strerror
    )
include_directories (${NGHTTP2_INCLUDE_DIRS})
target_link_libraries (${PROJECT_NAME} ${NGHTTP2_LIBRARIES})

set(PRIVATE_KEY_FILE ${PROJECT_BINARY_DIR}/server.key)
set(CERTIFICATE_CHAIN_FILE ${PROJECT_BINARY_DIR}/server.crt)
file(COPY server.key server.crt DESTINATION ${PROJECT_BINARY_DIR})



configure_file( DaaS_config.h.in DaaS_config.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

## Google Test
option( ${PROJECT_NAME}_BUILD_TESTS "Build the ${PROJECT_NAME} project tests." ON)
if (${PROJECT_NAME}_BUILD_TESTS)
    add_subdirectory(test)
endif (${PROJECT_NAME}_BUILD_TESTS)


# Debug
# get_cmake_property(_variableNames VARIABLES)
# foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()
