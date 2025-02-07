#include <cassert>
#include "snake.hpp"

// test_snake.cpp
void testSnake() {
    Snake s1(1, 2, 3);
    assert(s1.isValid());
    assert(s1.toString() == "Snake(x=1, y=2, len=3)");
    
    Snake s2(-1, 0, 1);
    assert(!s2.isValid());
}

int main() {
    testSnake();
}
