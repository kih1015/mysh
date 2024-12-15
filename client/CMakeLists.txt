cmake_minimum_required(VERSION 3.5)

project(TextStyleFileExplorer)

set(CMAKE_AUTOMOC ON)

# Find Qt5 modules
find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

# Source files
set(SOURCES
    main.cpp
    TextStyleFileExplorer.cpp
)

# Header files
set(HEADERS
    TextStyleFileExplorer.h
)

# Add the executable
add_executable(TextStyleFileExplorer ${SOURCES} ${HEADERS})

# Link Qt5 libraries
target_link_libraries(TextStyleFileExplorer Qt5::Widgets Qt5::Network)
