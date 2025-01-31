#include <zinc/openai.hpp>

#include <iostream>
#include <sstream>

int main(int argc, char **argv) {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    std::string_view url = "https://api.sambanova.ai";
    std::string_view model = "Meta-Llama-3.1-405B-Instruct";
    std::string_view key = "d8957211-24e6-426d-90cc-b267ce681e4f";
    zinc::OpenAI client(url, model, key);

    std::string prompt = "";
    for (int i = 1; i < argc; ++ i) {
        if (i > 1) prompt += " ";
        prompt += argv[i];
    }

    if (prompt.empty()) {
        std::cerr << "No arguments provided. Consuming stdin." << std::endl;
        std::stringstream ss;
        ss << std::cin.rdbuf();
        prompt = ss.str();
    }

    std::cout << prompt << std::flush;

    for (auto&& part : client.complete(prompt)) {
        std::cout << part << std::flush;
    }

    return 0;
}
