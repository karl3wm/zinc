#include <zinc/common.hpp>

#include <stdexcept>
#include <iostream>

using namespace zinc;

struct IntP
{
    static size_t leaks;

    IntP(int i)
    : storage(new int(i))
    { ++ leaks; }

    operator int()
    { return *storage; }

    ~IntP()
    {
        delete storage;
        -- leaks;
    }

    int* storage;
};
size_t IntP::leaks = 0;

generator<IntP> iterates_normally()
{
    co_yield 1;
    co_yield 2;
    co_yield 3;
}

generator<IntP> throws_initially()
{
    throw std::runtime_error("initial exception");
    co_yield 4;
}

generator<IntP> throws_later()
{
    co_yield 5;
    throw std::runtime_error("later exception");
}

generator<IntP> elements_of(generator<IntP>(gen)())
{
    co_yield ranges::elements_of(gen());
}

int main()
{
    int i;

    i = 0;
    for (int i_ : iterates_normally()) {
        i += i_;
    }
    if (i != 6) {
        throw std::logic_error("normal iteration test did not iterate expected value");
    }
    std::cerr << "normal iteration test passed" << std::endl;

    i = 0;
    for (int i_ : iterates_normally()) {
        i += i_;
        break;
    }
    if (i != 1) {
        throw std::logic_error("interrupted iteration test did not iterate expected value");
    }
    std::cerr << "interrupted iteration test passed" << std::endl;

    try {
        i = 0;
        for (int i_ : throws_later()) {
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
        for (int i_ : throws_initially()) {
            i += i_;
        }
        throw std::logic_error("generator initial exception never thrown");
    } catch(std::runtime_error&) {
        if (i != 0) {
            throw std::logic_error("generator initial exception iterated a value when it should not have");
        }
        std::cerr << "generator initial exception test passed" << std::endl;
    }

    if (IntP::leaks != 0) {
        throw std::logic_error("yielded values were leaked");
    }
    std::cerr << "yielded value leak test passed" << std::endl;

    i = 0;
    for (int i_ : elements_of(iterates_normally)) {
        i += i_;
    }
    if (i != 6) {
        throw std::logic_error("normal elements_of test did not iterate expected value");
    }
    std::cerr << "normal elemens_of test passed" << std::endl;

    i = 0;
    for (int i_ : elements_of(iterates_normally)) {
        i += i_;
        break;
    }
    if (i != 1) {
        throw std::logic_error("interrupted elements_of test did not iterate expected value");
    }
    std::cerr << "interrupted elements_of test passed" << std::endl;

    try {
        i = 0;
        for (int i_ : elements_of(throws_later)) {
            i += i_;
        }
        throw std::logic_error("elements_of iteration exception never thrown");
    } catch(std::runtime_error&) {
        if (i != 5) {
            throw std::logic_error("elements_of iteration exception test did not iterate expected value");
        }
        std::cerr << "elements_of iteration exception test passed" << std::endl;
    }

    try {
        i = 0;
        for (int i_ : elements_of(throws_initially)) {
            i += i_;
        }
        throw std::logic_error("elements_of initial exception never thrown");
    } catch(std::runtime_error&) {
        if (i != 0) {
            throw std::logic_error("elements_of initial exception iterated a value when it should not have");
        }
        std::cerr << "elements_of initial exception test passed" << std::endl;
    }

    if (IntP::leaks != 0) {
        throw std::logic_error("yielded values were leaked");
    }
    std::cerr << "yielded value leak test passed" << std::endl;
}
