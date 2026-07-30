add_executable(counter_bench counter_bench.cpp)
target_link_libraries(counter_bench PRIVATE Threads::Threads)
target_compile_options(counter_bench PRIVATE -O3 -march=native)

add_executable(pingpong_bench pingpong_bench.cpp)
target_link_libraries(pingpong_bench PRIVATE Threads::Threads)
target_compile_options(pingpong_bench PRIVATE -O3 -march=native)

add_executable(spsc_bench spsc_bench.cpp)
target_include_directories(spsc_bench PRIVATE ${CMAKE_SOURCE_DIR}/spsc)
target_link_libraries(spsc_bench PRIVATE Threads::Threads)
target_compile_options(spsc_bench PRIVATE -O3 -march=native)
