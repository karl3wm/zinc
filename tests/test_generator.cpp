#include <zinc/common.hpp>

#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace zinc;

struct IntP
{
    static size_t next_id;
    static std::unordered_set<size_t> leaked_ids;

    IntP(int i)
    : storage(new int(i))
    , id(next_id ++)
    { leaked_ids.insert(id); }

    IntP(IntP const & other) noexcept
    : storage(nullptr), id(~0u)
    { *this = other; }

    IntP(IntP && other) noexcept
    : storage(nullptr), id(~0u)
    { *this = std::move(other); }

    ~IntP() noexcept
    { clear(); }

    IntP& operator=(IntP const & other) noexcept
    {
        clear();
        if (other.storage == nullptr) {
            storage = nullptr;
        } else {
            storage = new int((int)other);
            id = next_id ++;
            leaked_ids.insert(id);
        }
        return *this;
    }

    IntP& operator=(IntP && other) noexcept
    {
        clear();
        storage = other.storage;
        id = other.id;
        other.clear_();
        return *this;
    }

    operator int() const noexcept
    { return *storage; }

    void clear() noexcept
    {
        if (storage != nullptr) {
            delete storage;
            leaked_ids.erase(id);
            clear_();
        }
    }

    void clear_() noexcept
    {
        storage = nullptr;
        id = ~0u;
    }

    int* storage;
    size_t id;
};
size_t IntP::next_id = 0;
std::unordered_set<size_t> IntP::leaked_ids{};

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

generator<IntP> empty_sequence()
{
    co_return;
}

generator<IntP> elements_of_(generator<IntP>(gen)())
{
    co_yield ranges::elements_of(gen());
}

generator<IntP> elements_of_elements_of(generator<IntP>(gen1)(), generator<IntP>(gen2)(), int i1, int i2)
{
    co_yield ranges::elements_of(elements_of_(gen1));
    co_yield i1;
    co_yield ranges::elements_of(elements_of_(gen2));
    co_yield i2;
}

int main()
{
    std::vector<int> expected, actual;

    // Test normal iteration
    expected = {1, 2, 3};
    actual.clear();
    for (int i_ : iterates_normally()) {
        actual.push_back(i_);
    }
    if (actual != expected) {
        throw std::logic_error("normal iteration test did not iterate expected values");
    }
    std::cerr << "normal iteration test passed" << std::endl;

    if (!IntP::leaked_ids.empty()) {
        throw std::logic_error("yielded values were leaked");
    }
    std::cerr << "yielded value leak test passed" << std::endl;

    // Test interrupted iteration
    expected = {1};
    actual.clear();
    for (int i_ : iterates_normally()) {
        actual.push_back(i_);
        break;
    }
    if (actual != expected) {
        throw std::logic_error("interrupted iteration test did not iterate expected values");
    }
    std::cerr << "interrupted iteration test passed" << std::endl;

    // Test iteration with exception
    expected = {5};
    actual.clear();
    try {
        for (int i_ : throws_later()) {
            actual.push_back(i_);
        }
        throw std::logic_error("generator iteration exception never thrown");
    } catch(std::runtime_error&) {
        if (actual != expected) {
            throw std::logic_error("generator iteration exception test did not iterate expected values");
        }
        std::cerr << "generator iteration exception test passed" << std::endl;
    }

    // Test initial exception
    expected = {};
    actual.clear();
    try {
        for (int i_ : throws_initially()) {
            actual.push_back(i_);
        }
        throw std::logic_error("generator initial exception never thrown");
    } catch(std::runtime_error&) {
        if (actual != expected) {
            throw std::logic_error("generator initial exception test did not iterate expected values");
        }
        std::cerr << "generator initial exception test passed" << std::endl;
    }

    if (!IntP::leaked_ids.empty()) {
        throw std::logic_error("yielded values were leaked");
    }
    std::cerr << "yielded value leak test passed" << std::endl;

    // Test elements_of
    expected = {1, 2, 3};
    actual.clear();
    for (int i_ : elements_of_(iterates_normally)) {
        actual.push_back(i_);
    }
    if (actual != expected) {
        throw std::logic_error("elements_of test did not iterate expected values");
    }
    std::cerr << "elements_of test passed" << std::endl;

    // Test interrupted elements_of
    expected = {1};
    actual.clear();
    for (int i_ : elements_of_(iterates_normally)) {
        actual.push_back(i_);
        break;
    }
    if (actual != expected) {
        throw std::logic_error("elements_of test did not iterate expected values");
    }
    std::cerr << "elements_of test passed" << std::endl;

    // Test elements_of with exception
    expected = {5};
    actual.clear();
    try {
        for (int i_ : elements_of_(throws_later)) {
            actual.push_back(i_);
        }
        throw std::logic_error("elements_of iteration exception never thrown");
    } catch(std::runtime_error&) {
        if (actual != expected) {
            throw std::logic_error("elements_of iteration exception test did not iterate expected values");
        }
        std::cerr << "elements_of iteration exception test passed" << std::endl;
    }

    // Test initial elements_of exception
    expected = {};
    actual.clear();
    try {
        for (int i_ : elements_of_(throws_initially)) {
            actual.push_back(i_);
        }
        throw std::logic_error("elements_of initial exception never thrown");
    } catch(std::runtime_error&) {
        if (actual != expected) {
            throw std::logic_error("elements_of initial exception test did not iterate expected values");
        }
        std::cerr << "elements_of initial exception test passed" << std::endl;
    }

    // Test 0-length sequence
    for ([[maybe_unused]]int i_ : empty_sequence()) {
        throw std::logic_error("empty sequence test iterated a value");
    }
    std::cerr << "empty sequence test passed" << std::endl;

    // Test elements_of with 0-length sequence
    for ([[maybe_unused]]int i_ : elements_of_(empty_sequence)) {
        throw std::logic_error("elements_of empty sequence test iterated a value");
    }
    std::cerr << "elements_of empty sequence test passed" << std::endl;

    // Test elements_of_elements_of
    expected = {1, 2, 3, 10, 1, 2, 3, 20};
    actual.clear();
    for (int i_ : elements_of_elements_of(iterates_normally, iterates_normally, 10, 20)) {
        actual.push_back(i_);
    }
    if (actual != expected) {
        throw std::logic_error("elements_of_elements_of test did not iterate expected values");
    }
    std::cerr << "elements_of_elements_of test passed" << std::endl;

    // Test elements_of_elements_of with exception
    expected = {1, 2, 3, 10, 5};
    actual.clear();
    try {
        for (int i_ : elements_of_elements_of(iterates_normally, throws_later, 10, 20)) {
            actual.push_back(i_);
        }
        throw std::logic_error("elements_of_elements_of iteration exception never thrown");
    } catch(std::runtime_error&) {
        if (actual != expected) {
            throw std::logic_error("elements_of_elements_of iteration exception test did not iterate expected values");
        }
        std::cerr << "elements_of_elements_of iteration exception test passed" << std::endl;
    }

    // Test elements_of_elements_of with initial exception
    expected = {1, 2, 3, 10};
    actual.clear();
    try {
        for (int i_ : elements_of_elements_of(iterates_normally, throws_initially, 10, 20)) {
            actual.push_back(i_);
        }
        throw std::logic_error("elements_of_elements_of initial exception never thrown");
    } catch(std::runtime_error&) {
        if (actual != expected) {
            throw std::logic_error("elements_of_elements_of initial exception test did not iterate expected value");
        }
        std::cerr << "elements_of_elements_of initial exception test passed" << std::endl;
    }

    if (!IntP::leaked_ids.empty()) {
        throw std::logic_error("yielded values were leaked");
    }
    std::cerr << "yielded value leak test passed" << std::endl;

    return 0;
}
