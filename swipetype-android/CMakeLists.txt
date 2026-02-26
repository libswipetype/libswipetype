cmake_minimum_required(VERSION 3.18)
project(swipetype-android-jni VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# JNI Shared Library
# ============================================================================

add_library(glide_jni SHARED
    src/main/cpp/GestureLibJNI.cpp
)

# Link against swipetype-core (built from sibling directory)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../swipetype-core
                 ${CMAKE_CURRENT_BINARY_DIR}/swipetype-core)

target_link_libraries(glide_jni
    PRIVATE
        swipetype-core
        log          # Android logging
)

target_include_directories(glide_jni
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../swipetype-core/include
)

# 16 KB page-size alignment required for Android 15+ (API 35+)
target_link_options(glide_jni PRIVATE -Wl,-z,max-page-size=16384)

# Strip in release for smaller .so
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(glide_jni PRIVATE -s)
endif()
