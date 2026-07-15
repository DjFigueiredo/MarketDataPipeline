add_executable(counter_bench counter_bench.cpp)
target_link_libraries(counter_bench PRIVATE Threads::Threads)

add_executable(pingpong_bench pingpong_bench.cpp)
target_link_libraries(pingpong_bench PRIVATE Threads::Threads)
