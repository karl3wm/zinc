#include <zinc/openai.hpp>

#include <iostream>
#include <vector>

int main([[maybe_unused]]int argc, [[maybe_unused]]char **argv) {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    std::string_view url = "https://api.sambanova.ai";
    std::string_view model = "Meta-Llama-3.1-405B-Instruct";
    std::string_view key = "d8957211-24e6-426d-90cc-b267ce681e4f";
    zinc::OpenAI client(url, model, key);

    std::vector<zinc::OpenAI::RoleContentPair> messages;

    std::string msg;

    while ("end of file not reached") {
        std::cerr << "user: " << std::flush;
        std::getline(std::cin, msg);
        if (!std::cin) {
            break;
        }
        messages.emplace_back("user", std::move(msg));
        // be nice to read more lines if there is content in cin's buffer
        // this would handle accidental pastes, for example

        std::cerr << "assistant: " << std::flush;
        for (auto&& part : client.chat(messages)) {
            msg += part;
            std::cout << part << std::flush;
        }
        std::cout << std::endl;
        messages.emplace_back("assistant", std::move(msg));
    }

    return 0;
}
