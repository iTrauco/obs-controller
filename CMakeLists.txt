cmake_minimum_required(VERSION 3.10)
project(obsbot_controller)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Add executable
add_executable(obsbot_controller 
    src/main.cpp
)

# Include directories
target_include_directories(obsbot_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

# Link directories
target_link_directories(obsbot_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/lib/x86_64-release
)

# Link libraries
target_link_libraries(obsbot_controller PRIVATE
    dev
    Threads::Threads
)