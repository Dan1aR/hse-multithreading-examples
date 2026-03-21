include(FetchContent)

if(NOT TARGET GTest::gtest)
    FetchContent_Declare(
        hw_future_googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(hw_future_googletest)
endif()

add_executable(hw_future_test future_test.cpp)
target_link_libraries(hw_future_test PRIVATE GTest::gtest GTest::gtest_main hw_future_lib Threads::Threads)

add_executable(hw_thread_pool_test thread_pool_test.cpp)
target_link_libraries(hw_thread_pool_test PRIVATE GTest::gtest GTest::gtest_main hw_future_lib Threads::Threads)

add_executable(hw_process_pool_test process_pool_test.cpp)
target_link_libraries(hw_process_pool_test PRIVATE GTest::gtest GTest::gtest_main hw_future_lib Threads::Threads)
