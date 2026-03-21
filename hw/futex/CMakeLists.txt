add_library(hw_futex_lib condition_variable.cpp)
target_include_directories(hw_futex_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hw_futex_lib PUBLIC Threads::Threads)

add_subdirectory(test)
