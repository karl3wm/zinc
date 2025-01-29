#include <array>
#include <iostream>

#include <zinc/openai.hpp>

using namespace zinc;

void test_completion(OpenAI& client) {
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

void test_chat(OpenAI& client) {
    auto messages = std::to_array<OpenAI::RoleContentPair>({
        {"user", "Hello"},
        {"assistant", "Hi there!"}
    });

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
    OpenAI client(
        "https://api.sambanova.ai", // url
        "Meta-Llama-3.1-405B-Instruct", // model
        "d8957211-24e6-426d-90cc-b267ce681e4f", // api key
        span<OpenAI::KeyJSONPair>({ // default parameters
            {"max_tokens", 4}
        })
    );

    // Run the tests
    test_completion(client);
    test_chat(client);

    return 0;
}
