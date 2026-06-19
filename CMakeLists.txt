cmake_minimum_required(VERSION 3.21)
project(GrammarRefine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Point CMake at your Qt 6 install. Either pass it on the command line:
#   cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.2/msvc2019_64"
# or uncomment and edit the line below.
# -----------------------------------------------------------------------------
# set(CMAKE_PREFIX_PATH "C:/Qt/6.7.2/msvc2019_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()   # handles AUTOMOC/AUTOUIC/AUTORCC (Qt 6.3+)

qt_add_executable(GrammarRefine
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/ServerManager.cpp
    src/ServerManager.h
    src/LlamaClient.cpp
    src/LlamaClient.h
)

target_include_directories(GrammarRefine PRIVATE src)
target_link_libraries(GrammarRefine PRIVATE Qt6::Widgets Qt6::Network)

set_target_properties(GrammarRefine PROPERTIES
    # No console window on Windows. Comment this out while debugging if you want
    # to see stdout in a terminal.
    WIN32_EXECUTABLE ON
)

# Copy the default config next to the built binary so the portable folder is
# self-contained.
add_custom_command(TARGET GrammarRefine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/config.json"
        "$<TARGET_FILE_DIR:GrammarRefine>/config.json"
)
