# Minimum CMake version required
cmake_minimum_required(VERSION 3.18)
# Project name and version
project(ZinC VERSION 0.1 LANGUAGES CXX)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Boost
find_package(Boost 1.83.0 REQUIRED COMPONENTS url json)
find_package(OpenSSL REQUIRED) # for boost networking

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

# Function to add a test executable
function(add_test_executable TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE zinc)
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
add_library(zinc SHARED ${LIB_SOURCES})
target_include_directories(zinc PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zinc PRIVATE ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

# Add tool binaries
file(GLOB TOOL_SOURCES "tools/*.cpp")
foreach(TOOL_SOURCE IN LISTS TOOL_SOURCES)
    get_filename_component(TOOL_NAME ${TOOL_SOURCE} NAME_WE)
    add_executable(${TOOL_NAME} ${TOOL_SOURCE})
    target_link_libraries(${TOOL_NAME} PRIVATE zinc)
endforeach()

# Optional: If you need to specify compiler-specific flags or requirements
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    option(ENABLE_ASAN "Enable AddressSanitizer" ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -Wall -Wextra -pedantic")
    if(ENABLE_ASAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -Werror -fsanitize=address,leak,undefined")
    else()
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -Werror -fsanitize=undefined")
    endif()
endif()

# Optional: Install targets if needed
# install(TARGETS zinc_main DESTINATION bin)
