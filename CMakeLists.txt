cmake_minimum_required(VERSION 3.16)
project(CacheCore VERSION 1.0.0 LANGUAGES CXX)

# ── C++ standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # -std=c++17, not -std=gnu++17

# ── Compiler warnings ────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4)
    # Suppress common false-positive MSVC warnings for portable C++
    add_compile_options(/wd4100)  # unreferenced formal parameter
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Threading (portable: pthreads on Linux/macOS, Win32 on Windows) ──────────
find_package(Threads REQUIRED)

# ── Library: cachecore_lib ───────────────────────────────────────────────────
# A static library containing the core LRUCache and Statistics logic.
# All three executables link against this.
add_library(cachecore_lib STATIC
    src/LRUCache.cpp
    src/Statistics.cpp
)
target_include_directories(cachecore_lib PUBLIC include)
target_link_libraries(cachecore_lib PUBLIC Threads::Threads)

# ── Executable: cachecore_cli ────────────────────────────────────────────────
add_executable(cachecore_cli examples/main.cpp)
target_link_libraries(cachecore_cli PRIVATE cachecore_lib)

# ── Executable: cachecore_tests ──────────────────────────────────────────────
add_executable(cachecore_tests
    tests/test_main.cpp
    tests/cache_tests.cpp
    tests/lru_tests.cpp
    tests/ttl_tests.cpp
)
# tests/ contains test_runner.h — expose it as a private include path
target_include_directories(cachecore_tests PRIVATE tests)
target_link_libraries(cachecore_tests PRIVATE cachecore_lib)

# Register with CTest so 'ctest' picks it up automatically
enable_testing()
add_test(NAME CacheCoreTests COMMAND cachecore_tests)

# ── Executable: cachecore_bench ──────────────────────────────────────────────
add_executable(cachecore_bench benchmarks/benchmark.cpp)
target_link_libraries(cachecore_bench PRIVATE cachecore_lib)

# ── Install (optional) ───────────────────────────────────────────────────────
install(TARGETS cachecore_cli DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/cachecore)
