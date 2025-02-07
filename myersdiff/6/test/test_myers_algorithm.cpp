// test_myers_algorithm.cpp
void testMyersAlgorithm() {
    std::deque<char> seq1{'A', 'B', 'C'};
    std::deque<char> seq2{'A', 'D', 'C'};
    DiffState state(3);
    
    MyersAlgorithm<char>::calculateDiff(seq1, seq2, state, true);
    
    const auto& snakes = state.getSnakes();
    assert(snakes.size() == 2);  // Should find A and C as snakes
    assert(snakes[0].toString() == "Snake(x=0, y=0, len=1)");  // A
    assert(snakes[1].toString() == "Snake(x=2, y=2, len=1)");  // C
}
#include <cassert>
#include "myers_algorithm.hpp"
#include "diff_state.hpp"
