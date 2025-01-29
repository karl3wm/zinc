#include <zinc/common.hpp>

#include <stdexcept>
#include <iostream>

using namespace zinc;

generator<int> throws_initially()
{
    throw std::runtime_error("initial exception");
    co_yield 4;
}

generator<int> throws_later()
{
    co_yield 5;
    throw std::runtime_error("later exception");
}

int main()
{
    int i = 0;
    try {
        for (int && i_ : throws_later()) {
            i += i_;
        }
        throw std::logic_error("generator iteration exception never thrown");
    } catch(std::runtime_error&) {
        if (i != 5) {
            throw std::logic_error("generator iteration exception test did not iterate expected value");
        }
        std::cerr << "generator iteration exception test passed" << std::endl;
    }

    try {
        i = 0;
        for (int && i_ : throws_initially()) {
            i += i_;
        }
        throw std::logic_error("generator initial exception never thrown");
    } catch(std::runtime_error&) {
        if (i != 0) {
            throw std::logic_error("generator initial exception iterated a value when it should not have");
        }
        std::cerr << "generator initial exception test passed" << std::endl;
    }
}

// vim: set ts=4 sw=4 et:
