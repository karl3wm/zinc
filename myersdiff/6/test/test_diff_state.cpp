#include <cassert>
#include "diff_state.hpp"

// test_diff_state.cpp
void testDiffState() {
    DiffState state(3);  // max_size = 3
    assert(state.dumpState().find("Forward vectors: 0 0 0 0 0 0 0") != std::string::npos);
    
    state[3] = 42;
    assert(state[3] == 42);
    
    state.addSnake(1, 1, 2);
    assert(state.getSnakes().size() == 1);
    assert(state.getSnakes()[0].toString() == "Snake(x=1, y=1, len=2)");
}

int main() {
    testDiffState();
}
