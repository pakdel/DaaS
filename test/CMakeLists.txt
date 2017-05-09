# https://github.com/google/googletest/blob/master/googletest/docs/Primer.md
# https://cmake.org/cmake/help/latest/module/FindGTest.html
set(PROJECT_NAME DaaS-tests)
enable_testing()
find_package(GTest REQUIRED)

file(GLOB TESTS *_test.hpp)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp redirect_stdout.hpp net_api_mock.hpp ${TESTS})

target_link_libraries(${PROJECT_NAME} GTest::GTest GTest::Main)
target_link_libraries(${PROJECT_NAME} pqxx)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT_NAME} ${MPI_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})

set(NGHTTP2_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/nghttp2/src/nghttp2/src/includes)
set(NGHTTP2_LIBRARIES    ${PROJECT_BINARY_DIR}/nghttp2/src/nghttp2-build/src/libnghttp2_asio.so
                         ${PROJECT_BINARY_DIR}/nghttp2/src/nghttp2-build/lib/libnghttp2.so  # For nghttp2_http2_strerror
    )
include_directories (${NGHTTP2_INCLUDE_DIRS})
target_link_libraries (${PROJECT_NAME} ${NGHTTP2_LIBRARIES})


add_test(AllDaaSTests DaaS-tests)
