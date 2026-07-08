cmake_minimum_required(VERSION 3.20)
project(MarketDataPipeline CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd / IDE

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)

# cmake -B build -DSANITIZE=address | thread | undefined
set(SANITIZE "" CACHE STRING "address|thread|undefined or empty")
if(SANITIZE)
  add_compile_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer -g -O1)
  add_link_options(-fsanitize=${SANITIZE})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)   # -O2/-O3, what benchmark numbers ship from
endif()

find_package(Threads REQUIRED)

add_subdirectory(bench)
# add_subdirectory(spsc)      # Week 4 — becomes a library; pipeline/ will link it
# add_subdirectory(lob)       # Week 7
# add_subdirectory(feed)      # Week 10
# add_subdirectory(pipeline)  # Week 10+ — target_link_libraries(pipeline spsc lob feed)
