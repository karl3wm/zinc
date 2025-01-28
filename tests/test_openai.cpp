#include "zinc/openai.hpp"
#include <iostream>
#include <vector>

void test_completion(zinc::OpenAI& client) {
    std::string prompt = "Once upon a time";
    std::cout << "Prompt: " << prompt << std::endl;

    size_t token_count = 0;
    for (auto&& completion : client.complete(prompt)) {
        std::cout << "Token: " << completion << std::endl;
        if (completion.size()) {
            ++ token_count;
        }
    }

    if (token_count <= 1) {
        std::cerr << "Test failed: Single completion did not generate more than one token." << std::endl;
    } else {
        std::cout << "Single completion test passed." << std::endl;
    }
}

void test_chat(zinc::OpenAI& client) {
    std::vector<std::pair<std::string_view, std::string_view>> messages = {
        {"user", "Hello"},
        {"assistant", "Hi there!"}
    };

    std::cout << "Chat Messages:" << std::endl;
    for ( auto & message : messages) {
        std::cout << "\t" << message.first << ": " << message.second << std::endl;
    }

    size_t token_count = 0;
    for (auto&& completion : client.chat(messages)) {
        std::cout << "Chat Token: " << completion << std::endl;
        if (completion.size()) {
            ++ token_count;
        }
    }

    if (token_count <= 1) {
        std::cerr << "Test failed: Single chat did not generate more than one token." << std::endl;
    } else {
        std::cout << "Single chat test passed." << std::endl;
    }
}

int main() {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    std::string url = "https://api.sambanova.ai";
    std::string model = "Meta-Llama-3.1-405B-Instruct";
    std::string key = "d8957211-24e6-426d-90cc-b267ce681e4f";

    zinc::OpenAI client(url, model, key, {{"max_tokens",4}});

    // Run the tests
    test_completion(client);
    test_chat(client);

    return 0;
}
