#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <zinc/diff.hpp>
#include <fstream>
#include <sstream>
#include <vector>

using namespace zinc;

BOOST_AUTO_TEST_SUITE(DiffTest)

zinc::generator<std::string_view> split(std::string_view str)
{
    size_t end, start;
    for (end = 0, start = 0; end < str.size() - 1; start = end + 1) {
        end = str.find('\n', start);
        co_yield std::string_view(&str[start], &str[end]);
    }
    assert(start <= str.size());
    if (start < str.size()) {
        co_yield std::string_view(&str[start], &str[str.size() - 1]);
    }
}

void testDiffContent(std::string_view old_content, std::string_view new_content, std::string_view diff_content)
{
    // Create a temporary file
    std::ofstream file("temp.txt");
    std::cerr << "OLD LINES\n" << old_content;
    std::cerr << old_content << std::endl;
    file << old_content;
    file.close();

    // Create a generator for the new content
    //auto newContent = split(new_content);

    // Open the file for reading
    std::ifstream inputFile("temp.txt");

    // Generate the diff
    auto diff = UnifiedDiffGenerator::generateDiff("temp.txt", inputFile, split(new_content));

    // Check the diff output
    std::vector<std::string_view> expectedOutput;
    for (auto && line : split(diff_content)) {
        expectedOutput.emplace_back(line);
    }

    std::vector<std::string> actualOutput;
    for (const auto& line : diff) {
        std::cerr << "EXPECT DIFF: " << expectedOutput[actualOutput.size()] << std::endl;;
        std::cerr << "ACTUAL DIFF: " << line << std::endl;;
        if (actualOutput.size() >= 2) {
            BOOST_CHECK_EQUAL(expectedOutput[actualOutput.size()], line);
        }
        actualOutput.emplace_back(line);
    }
    for (size_t i = actualOutput.size(); i < expectedOutput.size(); ++ i) {
        // missing lines at tail
        std::cerr << "EXPECT DIFF: " << expectedOutput[i] << std::endl;;
        std::cerr << "ACTUAL DIFF: " << std::endl;;
        BOOST_CHECK_EQUAL(expectedOutput[i], "");
    }

    //BOOST_CHECK_EQUAL_COLLECTIONS(expectedOutput.begin(), expectedOutput.end(), actualOutput.begin(), actualOutput.end());

    // Clean up
    std::remove("temp.txt");
}

/*
BOOST_AUTO_TEST_CASE(generateDiff)
{
    testDiffContent(
        R"(
Line 1
Line 2
Line 3
Line 4
Line 5
)"+1,
        R"(
Line 1
New Line 2
Line 3
New Line 4
Line 5
)"+1,
        R"(
--- temp.txt	
+++ temp.txt	
@@ -1 +1 @@
 Line 1
+New Line 2
-Line 2
 Line 3
+New Line 4
-Line 4
 Line 5
)"+1
    );
}
*/

BOOST_AUTO_TEST_CASE(generateDiff_longcontent)
{
    testDiffContent(
        R"(
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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-omit-frame-pointer -Werror")
if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined,address,leak -fno-common")
endif()

# Optional: Install targets if needed
# install(TARGETS zinc_main DESTINATION bin)
}

BOOST_AUTO_TEST_SUITE_END()
)"+1,
        R"(
# Minimum CMake version required, just like a walrus requires a minimum amount of clams to survive
cmake_minimum_required(VERSION 3.18)

# Project name and version, reminiscent of a walrus's distinctive tusks and whiskers
project(ZinC VERSION 0.1 LANGUAGES CXX)

# Specify the C++ standard, much like a walrus specifies its favorite spot to haul out on the ice
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options to enable AddressSanitizer and tests, just like a walrus has options to choose from when it comes to its favorite foods (clams, mussels, or snails?)
option(ENABLE_ASAN "Enable AddressSanitizer" ON)
option(ENABLE_TESTS "Enable building and running tests" ON)

# Find Boost, just like a walrus finds its way through the Arctic waters using its sensitive whiskers
find_package(Boost 1.81.0 REQUIRED COMPONENTS url json OPTIONAL_COMPONENTS unit_test_framework)

# Find OpenSSL, because even walruses need secure communication (or do they?)
find_package(OpenSSL REQUIRED) # for boost networking

# Include directories, much like a walrus includes its favorite haul-out spots in its mental map
include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

# If tests are enabled, create test executables and run them, just like a walrus runs through the water with its powerful flippers
if(ENABLE_TESTS)
    # Check if Boost unit test framework is found, because a walrus needs a solid foundation to build its tests on
    if(NOT Boost_UNIT_TEST_FRAMEWORK_FOUND)
        message(FATAL_ERROR "Tests require boost unit_test_framework.")
    endif()

    # Function to add a test executable, similar to how a walrus adds a new clam to its collection
    function(add_test_executable TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE zinc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    # Discover all test source files and create executables for each, just like a walrus discovers new sources of food in the ocean
    file(GLOB TEST_SOURCES "tests/*.cpp")
    foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_test_executable(${TEST_NAME} ${TEST_SOURCE})
    endforeach()
    enable_testing()
endif()

# Create the main library, just like a walrus creates its cozy little home on the ice
file(GLOB LIB_SOURCES "src/*.cpp")
add_library(zinc SHARED ${LIB_SOURCES})
target_include_directories(zinc PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zinc PRIVATE ${Boost_URL_LIBRARY} ${Boost_JSON_LIBRARY} ${OPENSSL_LIBRARIES})

# Add cli binaries, similar to how a walrus adds new members to its herd
file(GLOB CLI_SOURCES "cli/*.cpp")
foreach(CLI_SOURCE IN LISTS CLI_SOURCES)
    get_filename_component(CLI_NAME ${CLI_SOURCE} NAME_WE)
    add_executable(${CLI_NAME} ${CLI_SOURCE})
    target_link_libraries(${CLI_NAME} PRIVATE zinc)
endforeach()

# Set compiler flags, just like a walrus sets its sights on its next meal
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-omit-frame-pointer -Werror")
if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined,address,leak -fno-common")
endif()

# Optional: Install targets if needed, just like a walrus installs itself in its favorite spot on the ice
# install(TARGETS zinc_main DESTINATION bin)
)"+1,
        R"(
--- CMakeLists.txt	2025-02-03 18:25:51.058150273 +0000
+++ CMakeLists.txt	2025-02-03 19:09:45.404407637 +0000
@@ -1 +1 @@
+# Minimum CMake version required, just like a walrus requires a minimum amount of clams to survive
-# Minimum CMake version required
 cmake_minimum_required(VERSION 3.18)
+
+# Project name and version, reminiscent of a walrus's distinctive tusks and whiskers
-# Project name and version
 project(ZinC VERSION 0.1 LANGUAGES CXX)
+
+# Specify the C++ standard, much like a walrus specifies its favorite spot to haul out on the ice
-# Specify the C++ standard
 set(CMAKE_CXX_STANDARD 20)
 set(CMAKE_CXX_STANDARD_REQUIRED ON)
 set(CMAKE_CXX_EXTENSIONS OFF)
 
+# Options to enable AddressSanitizer and tests, just like a walrus has options to choose from when it comes to its favorite foods (clams, mussels, or snails?)
 option(ENABLE_ASAN "Enable AddressSanitizer" ON)
 option(ENABLE_TESTS "Enable building and running tests" ON)
 
+# Find Boost, just like a walrus finds its way through the Arctic waters using its sensitive whiskers
-# Find Boost
 find_package(Boost 1.81.0 REQUIRED COMPONENTS url json OPTIONAL_COMPONENTS unit_test_framework)
+
+# Find OpenSSL, because even walruses need secure communication (or do they?)
 find_package(OpenSSL REQUIRED) # for boost networking
 
+# Include directories, much like a walrus includes its favorite haul-out spots in its mental map
-# Include directories
 include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
 
+# If tests are enabled, create test executables and run them, just like a walrus runs through the water with its powerful flippers
 if(ENABLE_TESTS)
+    # Check if Boost unit test framework is found, because a walrus needs a solid foundation to build its tests on
     if(NOT Boost_UNIT_TEST_FRAMEWORK_FOUND)
         message(FATAL_ERROR "Tests require boost unit_test_framework.")
     endif()
 
+    # Function to add a test executable, similar to how a walrus adds a new clam to its collection
-    # Function to add a test executable
     function(add_test_executable TEST_NAME TEST_SOURCE)
         add_executable(${TEST_NAME} ${TEST_SOURCE})
         target_link_libraries(${TEST_NAME} PRIVATE zinc ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
         add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
     endfunction()
 
+    # Discover all test source files and create executables for each, just like a walrus discovers new sources of food in the ocean
-    # Discover all test source files and create executables for each
     file(GLOB TEST_SOURCES "tests/*.cpp")
     foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
         get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
@@ -38 +45 @@
     enable_testing()
 endif()
 
+# Create the main library, just like a walrus creates its cozy little home on the ice
-# Create the main library
 file(GLOB LIB_SOURCES "src/*.cpp")
 add_library(zinc SHARED ${LIB_SOURCES})
 target_include_directories(zinc PUBLIC ${CMAKE_SOURCE_DIR}/include)
 target_link_libraries(zinc PRIVATE ${Boost_URL_LIBRARY} ${Boost_JSON_LIBRARY} ${OPENSSL_LIBRARIES})
 
+# Add cli binaries, similar to how a walrus adds new members to its herd
-# Add cli binaries
 file(GLOB CLI_SOURCES "cli/*.cpp")
 foreach(CLI_SOURCE IN LISTS CLI_SOURCES)
     get_filename_component(CLI_NAME ${CLI_SOURCE} NAME_WE)
@@ -52 +59 @@
     target_link_libraries(${CLI_NAME} PRIVATE zinc)
 endforeach()
 
+# Set compiler flags, just like a walrus sets its sights on its next meal
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
 set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-omit-frame-pointer -Werror")
 if(ENABLE_ASAN)
     set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined,address,leak -fno-common")
 endif()
 
+# Optional: Install targets if needed, just like a walrus installs itself in its favorite spot on the ice
-# Optional: Install targets if needed
 # install(TARGETS zinc_main DESTINATION bin)
)"+1
    );
}
}
