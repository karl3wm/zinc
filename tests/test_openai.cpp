#include "zinc/openai.hpp"
#include <iostream>
#include <vector>

void test_single_completion(zinc::OpenAI& client) {
    std::string prompt = "Once upon a time";
    auto completion_gen = client.gen_completion(prompt);

    size_t token_count = 0;
    for (auto&& completion : completion_gen) {
        std::cout << "Token: " << static_cast<std::string_view>(completion) << std::endl;
        ++token_count;
        if (token_count > 1) {
            // Ensure more than one token is generated
            break;
        }
    }

    if (token_count <= 1) {
        std::cerr << "Test failed: Single completion did not generate more than one token." << std::endl;
    } else {
        std::cout << "Single completion test passed." << std::endl;
    }
}

void test_multiple_completions(zinc::OpenAI& client) {
    std::string prompt = "Once upon a time";
    size_t num_completions = 3;
    auto completions_gen = client.gen_completions(prompt, num_completions);

    size_t token_count = 0;
    for (auto&& span : completions_gen) {
        for (auto&& completion : span) {
            std::cout << "Completion Token: " << static_cast<std::string_view>(completion) << std::endl;
            ++token_count;
        }
        if (token_count >= num_completions) {
            // Ensure at least one token per completion is generated
            break;
        }
    }

    if (token_count < num_completions) {
        std::cerr << "Test failed: Multiple completions did not generate enough tokens." << std::endl;
    } else {
        std::cout << "Multiple completions test passed." << std::endl;
    }
}

void test_single_chat(zinc::OpenAI& client) {
    std::vector<std::pair<std::string_view, std::string_view>> messages = {
        {"user", "Hello"},
        {"assistant", "Hi there!"}
    };
    auto chat_gen = client.gen_chat(std::span(messages));

    size_t token_count = 0;
    for (auto&& completion : chat_gen) {
        std::cout << "Chat Token: " << static_cast<std::string_view>(completion) << std::endl;
        ++token_count;
        if (token_count > 1) {
            // Ensure more than one token is generated
            break;
        }
    }

    if (token_count <= 1) {
        std::cerr << "Test failed: Single chat did not generate more than one token." << std::endl;
    } else {
        std::cout << "Single chat test passed." << std::endl;
    }
}

void test_multiple_chats(zinc::OpenAI& client) {
    std::vector<std::pair<std::string_view, std::string_view>> messages = {
        {"user", "Hello"},
        {"assistant", "Hi there!"}
    };
    size_t num_chats = 2;
    auto chats_gen = client.gen_chats(std::span(messages), num_chats);

    size_t token_count = 0;
    for (auto&& span : chats_gen) {
        for (auto&& completion : span) {
            std::cout << "Chat Completion Token: " << static_cast<std::string_view>(completion) << std::endl;
            ++token_count;
        }
        if (token_count >= num_chats) {
            // Ensure at least one token per chat is generated
            break;
        }
    }

    if (token_count < num_chats) {
        std::cerr << "Test failed: Multiple chats did not generate enough tokens." << std::endl;
    } else {
        std::cout << "Multiple chats test passed." << std::endl;
    }
}

int main() {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    std::string url = "https://api.sambanova.ai";
    std::string model = "Meta-Llama-3.1-405B-Instruct";
    std::string key = "d8957211-24e6-426d-90cc-b267ce681e4f";

    zinc::OpenAI client(url, model, key);

    // Run the tests
    test_single_completion(client);
    test_multiple_completions(client);
    test_single_chat(client);
    test_multiple_chats(client);

    return 0;
}
