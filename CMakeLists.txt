cmake_minimum_required(VERSION 3.28)
project(proc-manager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──
option(BUILD_TESTS "Build tests" ON)
option(USE_TUI "Build with FTXUI TUI support" ON)

# ── Dependencies ──
include(FetchContent)

# spdlog (header-only logging)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
)
FetchContent_MakeAvailable(spdlog)

# CLI11 (argument parsing)
FetchContent_Declare(CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.5.0
)
FetchContent_MakeAvailable(CLI11)

# FTXUI (for TUI mode)
if(USE_TUI)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    SYSTEM
  )
  FetchContent_MakeAvailable(ftxui)
endif()

# ── Library (all source except main) ──
file(GLOB_RECURSE LIB_SRCS src/*.cpp)
list(FILTER LIB_SRCS EXCLUDE REGEX "main\\.cpp$")
file(GLOB_RECURSE HEADERS include/*.hpp)

add_library(procman-lib STATIC ${LIB_SRCS} ${HEADERS})
target_include_directories(procman-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(procman-lib PUBLIC spdlog::spdlog CLI11::CLI11)

# ── Executable ──
add_executable(proc-manager src/main.cpp)
target_include_directories(proc-manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(proc-manager PRIVATE procman-lib)

if(USE_TUI)
  target_link_libraries(proc-manager PRIVATE ftxui::screen ftxui::dom ftxui::component)
  target_compile_definitions(proc-manager PRIVATE PROCMAN_USE_TUI)
endif()

# ── Tests ──
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ──
install(TARGETS proc-manager DESTINATION bin)
