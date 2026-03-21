include(FetchContent)

FetchContent_Declare(
    hw_googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(hw_googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    hw_googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
)
FetchContent_MakeAvailable(hw_googlebenchmark)

add_executable(hw_ipc_test mpsc_queue_test.cpp)
target_link_libraries(hw_ipc_test PRIVATE Threads::Threads GTest::gtest
                                           GTest::gtest_main hw_ipc_lib)

add_executable(hw_ipc_benchmark mpsc_queue_benchmark.cpp)
target_link_libraries(hw_ipc_benchmark PRIVATE Threads::Threads benchmark::benchmark
                                                hw_ipc_lib)
