include(FetchContent)

if(NOT TARGET GTest::gtest)
    FetchContent_Declare(
        hw_futex_googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(hw_futex_googletest)
endif()

add_executable(hw_futex_condition_variable_test condition_variable_test.cpp)
target_link_libraries(
    hw_futex_condition_variable_test
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hw_futex_lib
)
