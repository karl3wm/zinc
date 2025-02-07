# Minimum CMake version required
cmake_minimum_required(VERSION 3.18)
# Project name and version
project(ZinC VERSION 0.1 LANGUAGES CXX)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ASAN "Enable AddressSanitizer" ON)
option(ENABLE_TESTS "Enable building and running tests" ON)

# Find Boost
find_package(Boost 1.81.0 REQUIRED COMPONENTS url json OPTIONAL_COMPONENTS unit_test_framework)
find_package(OpenSSL REQUIRED) # for boost networking

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

if(ENABLE_TESTS)
    if(NOT Boost_UNIT_TEST_FRAMEWORK_FOUND)
        message(FATAL_ERROR "Tests require boost unit_test_framework.")
    endif()

    # Function to add a test executable
    function(add_test_executable TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE zinc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    # Discover all test source files and create executables for each
    file(GLOB TEST_SOURCES "tests/*.cpp")
    foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_test_executable(${TEST_NAME} ${TEST_SOURCE})
    endforeach()
    enable_testing()
endif()

# Create the main library
file(GLOB LIB_SOURCES "src/*.cpp")
add_library(zinc SHARED ${LIB_SOURCES})
target_include_directories(zinc PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zinc PRIVATE ${Boost_URL_LIBRARY} ${Boost_JSON_LIBRARY} ${OPENSSL_LIBRARIES})

# Add cli binaries
file(GLOB CLI_SOURCES "cli/*.cpp")
foreach(CLI_SOURCE IN LISTS CLI_SOURCES)
    get_filename_component(CLI_NAME ${CLI_SOURCE} NAME_WE)
    add_executable(${CLI_NAME} ${CLI_SOURCE})
    target_link_libraries(${CLI_NAME} PRIVATE zinc)
endforeach()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra --pedantic-errors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wsign-conversion")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-omit-frame-pointer -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-keyword-macro")
if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined,address,leak -fno-common")
endif()

# Optional: Install targets if needed
# install(TARGETS zinc_main DESTINATION bin)
