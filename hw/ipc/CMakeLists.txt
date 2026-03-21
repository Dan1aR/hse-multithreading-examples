add_library(hw_ipc_lib INTERFACE)
target_include_directories(hw_ipc_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(hw_ipc_producer producer_demo.cpp)
target_link_libraries(hw_ipc_producer PRIVATE Threads::Threads hw_ipc_lib rt)

add_executable(hw_ipc_consumer consumer_demo.cpp)
target_link_libraries(hw_ipc_consumer PRIVATE Threads::Threads hw_ipc_lib rt)

add_subdirectory(test)
