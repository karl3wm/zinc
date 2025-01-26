# Minimum CMake version required
cmake_minimum_required(VERSION 3.18)

# Project name and version
project(ZinC VERSION 0.1 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Boost
find_package(Boost 1.83.0 REQUIRED COMPONENTS url)
find_package(OpenSSL REQUIRED) # for boost networking

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

# Find the nlohmann_json package
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    add_subdirectory(third_party/json)
endif()

# Function to add a test executable
function(add_test_executable TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
    target_link_libraries(${TEST_NAME} PRIVATE zinc_lib ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Discover all test source files and create executables for each
file(GLOB TEST_SOURCES "tests/*.cpp")
foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_test_executable(${TEST_NAME} ${TEST_SOURCE})
endforeach()

# Create the main library
file(GLOB LIB_SOURCES "src/*.cpp")
add_library(zinc_lib ${LIB_SOURCES})
target_include_directories(zinc_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zinc_lib PRIVATE ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} nlohmann_json::nlohmann_json)

# Add tool binaries
#file(GLOB TOOL_SOURCES "tools/*.cpp") # Assuming you have tools in a 'tools' directory
#foreach(TOOL_SOURCE IN LISTS TOOL_SOURCES)
#    get_filename_component(TOOL_NAME ${TOOL_SOURCE} NAME_WE)
#    add_executable(${TOOL_NAME} ${TOOL_SOURCE})
#    target_link_libraries(${TOOL_NAME} PRIVATE zinc_lib ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
#endforeach()

# Optional: If you need to specify compiler-specific flags or requirements
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    execute_process(COMMAND g++-14 --version RESULT_VARIABLE GXX_14_RESULT OUTPUT_QUIET ERROR_QUIET)
    if(GXX_14_RESULT EQUAL 0)
        set(CMAKE_CXX_COMPILER "g++-14" CACHE STRING "C++ compiler" FORCE)
    else()
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
            message(FATAL_ERROR "GCC version 14 or higher is required. Install g++-14 if available.")
        endif()
    endif()
    # Add any specific GCC flags here if needed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fsanitize=address,leak,undefined")
endif()

# Optional: Install targets if needed
# install(TARGETS zinc_main DESTINATION bin)
