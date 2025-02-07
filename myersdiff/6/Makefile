CC=g++
CFLAGS=-std=c++11 -Wall -Werror
INCLUDES=-Iinclude

test_snake: test/test_snake.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test_diff_state: test/test_diff_state.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test_myers_algorithm: test/test_myers_algorithm.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

all: test_snake test_diff_state test_myers_algorithm

clean:
	rm -f test_snake test_diff_state test_myers_algorithm
