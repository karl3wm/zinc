// snake.hpp
#pragma once
#include <string>
#include <sstream>

struct Snake {
    int x, y, len;
    
    Snake(int x = 0, int y = 0, int len = 0) : x(x), y(y), len(len) {}
    
    std::string toString() const {
        std::stringstream ss;
        ss << "Snake(x=" << x << ", y=" << y << ", len=" << len << ")";
        return ss.str();
    }
    
    bool isValid() const {
        return x >= 0 && y >= 0 && len >= 0;
    }
};

