# Minimum CMake version required
cmake_minimum_required(VERSION 3.18)

# Project name and version
project(ZinC VERSION 0.1 LANGUAGES CXX C)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ASAN "Enable AddressSanitizer" ON)
option(ENABLE_TESTS "Enable building and running tests" ON)
option(USE_PYTHON_EMBEDDED "Use embedded Python interpreter" ON)

# Find Boost components required for the main library
find_package(Boost 1.81.0 REQUIRED COMPONENTS url json)
find_package(OpenSSL REQUIRED) # for boost networking

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

# Conditionally enable tests
if(ENABLE_TESTS)
    # Define a list of Boost components required for tests
    set(BOOST_TEST_COMPONENTS unit_test_framework)

    # Find Boost components required for tests
    find_package(Boost 1.81.0 COMPONENTS ${BOOST_TEST_COMPONENTS})

    # Check if all Boost test components are found
    foreach(component IN LISTS BOOST_TEST_COMPONENTS)
        if(NOT Boost_${component}_FOUND)
            message(FATAL_ERROR "Tests require the Boost ${component} component. Please install it or disable tests by setting ENABLE_TESTS to OFF.")
        endif()
    endforeach()

    # Function to add a test executable
    function(add_test_executable TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE zinc)
        # Link against all Boost test components
        foreach(component IN LISTS BOOST_TEST_COMPONENTS)
            target_link_libraries(${TEST_NAME} PRIVATE Boost::${component})
        endforeach()
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
file(GLOB LIB_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} CONFIGURE_DEPENDS "src/*.cpp")

set(LIB_DEPENDENCIES
    ${Boost_URL_LIBRARY}
    ${Boost_JSON_LIBRARY}
    ${OPENSSL_LIBRARIES}
)

if (USE_PYTHON_EMBEDDED)
    list(REMOVE_ITEM LIB_SOURCES src/python_subprocess.cpp)
    find_package(Python REQUIRED COMPONENTS Development.Embed)
    list(APPEND LIB_DEPENDENCIES Python::Python)
else ()
    list(REMOVE_ITEM LIB_SOURCES src/python_embedded.cpp)
    find_package(Python REQUIRED COMPONENTS Interpreter)
endif ()

add_library(zinc SHARED ${LIB_SOURCES})
target_include_directories(zinc PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zinc PRIVATE ${LIB_DEPENDENCIES})

# Create a separate library target for xdiff
file(GLOB XDIFF_SOURCES "third_party/xdiff/*.c")
add_library(xdiff STATIC ${XDIFF_SOURCES})
target_compile_options(xdiff PRIVATE -fPIC)
target_include_directories(xdiff PUBLIC "third_party")

# Link xdiff to the main library
target_link_libraries(zinc PRIVATE xdiff)

# Add cli binaries
file(GLOB CLI_SOURCES "cli/*.cpp")
foreach(CLI_SOURCE IN LISTS CLI_SOURCES)
    get_filename_component(CLI_NAME ${CLI_SOURCE} NAME_WE)
    add_executable(${CLI_NAME} ${CLI_SOURCE})
    target_link_libraries(${CLI_NAME} PRIVATE zinc)
endforeach()

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra --pedantic-errors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wsign-conversion")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-omit-frame-pointer -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-elide-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-keyword-macro -Wno-comment")
if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined,address,leak -fno-common")
endif()

# Optional: Install targets if needed
# install(TARGETS zinc_main DESTINATION bin)
