cmake_minimum_required(VERSION 3.28)
project(tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Catch2 from FetchContent (the parent's FetchContent handles it)
include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_procman test_main.cpp)
target_include_directories(test_procman PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_procman PRIVATE
  Catch2::Catch2WithMain
  procman-lib
)

include(CTest)
add_test(NAME test_procman COMMAND test_procman)
