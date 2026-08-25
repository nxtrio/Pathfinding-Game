cmake_minimum_required(VERSION 3.28)

project(PathfindingGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SFML 3 REQUIRED COMPONENTS Graphics)

add_executable(PathfindingGame
    src/Main.cpp
    src/PathfindingGame.cpp
)

target_include_directories(PathfindingGame PRIVATE include)
target_link_libraries(PathfindingGame PRIVATE SFML::Graphics)

add_custom_command(TARGET PathfindingGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:PathfindingGame>/assets"
    COMMENT "Copying game assets"
)

include(CTest)

if(BUILD_TESTING)
    add_executable(PathfindingGameTests
        tests/PathfindingGameTests.cpp
        src/PathfindingGame.cpp
    )

    target_include_directories(PathfindingGameTests PRIVATE include)
    target_link_libraries(PathfindingGameTests PRIVATE SFML::Graphics)

    add_test(NAME PathfindingGameAlgorithms COMMAND PathfindingGameTests)
endif()
