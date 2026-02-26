cmake_minimum_required(VERSION 3.18)
project(swipetype-core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# Release optimization
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

# ============================================================================
# Core Library
# ============================================================================

set(SWIPETYPE_CORE_SOURCES
    src/PathProcessor.cpp
    src/IdealPathGenerator.cpp
    src/Scorer.cpp
    src/DictionaryLoader.cpp
    src/GestureEngine.cpp
    src/AdjacencyMap.cpp
)

set(SWIPETYPE_CORE_HEADERS
    include/swipetype/SwipeTypeTypes.h
    include/swipetype/GesturePoint.h
    include/swipetype/GesturePath.h
    include/swipetype/GestureCandidate.h
    include/swipetype/KeyboardLayout.h
    include/swipetype/PathProcessor.h
    include/swipetype/IdealPathGenerator.h
    include/swipetype/Scorer.h
    include/swipetype/DictionaryLoader.h
    include/swipetype/GestureEngine.h
)

add_library(swipetype-core STATIC ${SWIPETYPE_CORE_SOURCES} ${SWIPETYPE_CORE_HEADERS})

target_include_directories(swipetype-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link Android log library when building for Android
if(ANDROID)
    find_library(log-lib log)
    target_link_libraries(swipetype-core PUBLIC ${log-lib})
endif()

# ============================================================================
# Tests (Google Test)
# ============================================================================

option(SWIPETYPE_BUILD_TESTS "Build unit tests" ON)

if(SWIPETYPE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Install (for use as a CMake package)
# ============================================================================

install(TARGETS swipetype-core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/swipetype/
    DESTINATION include/swipetype
    FILES_MATCHING PATTERN "*.h"
)
