add_executable(cas_spinlock cas_spinlock.cpp)
target_link_libraries(cas_spinlock PRIVATE Threads::Threads)

add_executable(broken_aba broken_aba.cpp)
target_link_libraries(broken_aba PRIVATE Threads::Threads)