# Minimum CMake version required
cmake_minimum_required(VERSION 3.18)

# Project name and version
project(ZinC VERSION 0.1 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the output directories for binaries and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")

# Add the main executable (if you have one, otherwise remove this block)
add_executable(zinc_main src/http_beast.cpp)

# Link any necessary libraries here if needed
# target_link_libraries(zinc_main ...)

# Add tests
if(TEST_SOURCES)
    enable_testing()
    add_executable(test_zinc ${TEST_SOURCES})
    target_include_directories(test_zinc PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test_zinc PRIVATE zinc_main) # Link against your main library/executable if necessary

    # Use CTest to run the tests
    add_test(NAME test_zinc COMMAND test_zinc)
endif()

# Optional: If you need to specify compiler-specific flags or requirements
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
        message(FATAL_ERROR "GCC version 14 or higher is required.")
    endif()
    # Add any specific GCC flags here if needed
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Optional: Install targets if needed
# install(TARGETS zinc_main DESTINATION bin)
