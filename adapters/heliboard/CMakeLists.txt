cmake_minimum_required(VERSION 3.18)
project(heliboard-adapter-jni VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(heliboard_glide_jni SHARED
    src/main/cpp/HeliboardJNIBridge.cpp
)

target_link_libraries(heliboard_glide_jni
    PRIVATE
        log
)

# Note: For the drop-in .so approach, this would also link against swipetype-core.
# For the MVP Java-level approach, the native bridge is optional.
