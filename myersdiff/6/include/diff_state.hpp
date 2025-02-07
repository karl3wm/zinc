// diff_state.hpp
#pragma once
#include <vector>
#include <string>
#include "snake.hpp"

class DiffState {
public:
    explicit DiffState(int max_size) {
        // Size = 2 * (max_size + 1) + 1
        forward_vectors_.resize(2 * max_size + 1, 0);
    }
    
    void resetVectors() {
        std::fill(forward_vectors_.begin(), forward_vectors_.end(), 0);
    }
    
    void addSnake(int x, int y, int len) {
        snakes_.emplace_back(x, y, len);
        DiffTrace::log("Added " + snakes_.back().toString());
    }
    
    int& operator[](int k) { return forward_vectors_[k]; }
    const int& operator[](int k) const { return forward_vectors_[k]; }
    
    const std::vector<Snake>& getSnakes() const { return snakes_; }
    void clearSnakes() { snakes_.clear(); }
    
    std::string dumpState() const {
        std::stringstream ss;
        ss << "Forward vectors: ";
        for (int x : forward_vectors_) ss << x << " ";
        ss << "\nSnakes:\n";
        for (const auto& snake : snakes_) {
            ss << "  " << snake.toString() << "\n";
        }
        return ss.str();
    }
    
private:
    std::vector<int> forward_vectors_;
    std::vector<Snake> snakes_;
};

