#!/bin/bash

# Create directories
mkdir -p include src test

# Separate code into files
echo "// snake.hpp" > include/snake.hpp
sed -n '/\/\/ snake.hpp/,/\/\/ diff_state.hpp/p' files.txt | sed '1d;$d' >> include/snake.hpp

echo "// diff_state.hpp" > include/diff_state.hpp
sed -n '/\/\/ diff_state.hpp/,/\/\/ myers_algorithm.hpp/p' files.txt | sed '1d;$d' >> include/diff_state.hpp

echo "// myers_algorithm.hpp" > include/myers_algorithm.hpp
sed -n '/\/\/ myers_algorithm.hpp/,/\/\/ test_snake.cpp/p' files.txt | sed '1d;$d' >> include/myers_algorithm.hpp

echo "// test_snake.cpp" > test/test_snake.cpp
sed -n '/\/\/ test_snake.cpp/,/\/\/ test_diff_state.cpp/p' files.txt | sed '1d;$d' >> test/test_snake.cpp

echo "// test_diff_state.cpp" > test/test_diff_state.cpp
sed -n '/\/\/ test_diff_state.cpp/,/\/\/ test_myers_algorithm.cpp/p' files.txt | sed '1d;$d' >> test/test_diff_state.cpp

echo "// test_myers_algorithm.cpp" > test/test_myers_algorithm.cpp
sed -n '/\/\/ test_myers_algorithm.cpp/,$p' files.txt | sed '1d' >> test/test_myers_algorithm.cpp

# Add missing includes and namespace
echo "#include <cassert>" >> test/test_snake.cpp
echo "#include <cassert>" >> test/test_diff_state.cpp
echo "#include <cassert>" >> test/test_myers_algorithm.cpp
echo "#include \"snake.hpp\"" >> test/test_snake.cpp
echo "#include \"diff_state.hpp\"" >> test/test_diff_state.cpp
echo "#include \"myers_algorithm.hpp\"" >> test/test_myers_algorithm.cpp
echo "#include \"diff_state.hpp\"" >> test/test_myers_algorithm.cpp

# Create a Makefile
echo "CC=g++" > Makefile
echo "CFLAGS=-std=c++11 -Wall -Werror" >> Makefile
echo "INCLUDES=-Iinclude" >> Makefile
echo "" >> Makefile
echo "test_snake: test/test_snake.cpp" >> Makefile
echo "    \$(CC) \$(CFLAGS) \$(INCLUDES) -o \$@ \$^" >> Makefile
echo "" >> Makefile
echo "test_diff_state: test/test_diff_state.cpp" >> Makefile
echo "    \$(CC) \$(CFLAGS) \$(INCLUDES) -o \$@ \$^" >> Makefile
echo "" >> Makefile
echo "test_myers_algorithm: test/test_myers_algorithm.cpp" >> Makefile
echo "    \$(CC) \$(CFLAGS) \$(INCLUDES) -o \$@ \$^" >> Makefile
echo "" >> Makefile
echo "all: test_snake test_diff_state test_myers_algorithm" >> Makefile
echo "" >> Makefile
echo "clean:" >> Makefile
echo "    rm -f test_snake test_diff_state test_myers_algorithm" >> Makefile
