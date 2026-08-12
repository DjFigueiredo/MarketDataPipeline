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


if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Boost REQUIRED)

    add_executable(spsc_head_to_head spsc_head_to_head.cpp)
    target_include_directories(spsc_head_to_head PRIVATE
        ${CMAKE_SOURCE_DIR}/spsc
    )
    target_include_directories(spsc_head_to_head SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/rigtorp/include
        ${CMAKE_SOURCE_DIR}/third_party/readerwriterqueue
        ${CMAKE_SOURCE_DIR}/third_party/folly
        ${Boost_INCLUDE_DIRS}
    )
    target_link_libraries(spsc_head_to_head PRIVATE Threads::Threads)
    target_compile_options(spsc_head_to_head PRIVATE -O3 -march=native)
    target_compile_definitions(spsc_head_to_head PRIVATE FOLLY_NO_CONFIG)
endif()
