# Test executable
set(SWIPETYPE_TEST_SOURCES
    PathProcessorTest.cpp
    ScorerTest.cpp
    DictionaryLoaderTest.cpp
    GestureEngineTest.cpp
    IdealPathGeneratorTest.cpp
)

add_executable(swipetype-core-tests ${SWIPETYPE_TEST_SOURCES})

target_link_libraries(swipetype-core-tests
    PRIVATE
        swipetype-core
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(swipetype-core-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(swipetype-core-tests)
