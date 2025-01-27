#include <zinc/openai.hpp>

#include <iostream>
#include <vector>

int main(int argc, char **argv) {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    std::string url = "https://api.sambanova.ai";
    std::string model = "Meta-Llama-3.1-405B-Instruct";
    std::string key = "d8957211-24e6-426d-90cc-b267ce681e4f";

    zinc::OpenAI client(url, model, key);

    std::string prompt = "";
    for (int i = 1; i < argc; ++ i) {
        prompt += argv[i];
        prompt += " ";
    }

    std::cout << prompt << std::flush;

    for (auto&& part : client.gen_completion(prompt)) {
        std::cout << part << std::flush;
    }

    return 0;
}
