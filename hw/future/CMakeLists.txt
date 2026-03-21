add_library(hw_future_lib INTERFACE)
target_include_directories(hw_future_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(hw_future main.cpp)
target_link_libraries(hw_future PRIVATE Threads::Threads hw_future_lib)

add_subdirectory(test)
