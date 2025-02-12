#include <zinc/openai.hpp>
#include <zinc/log.hpp>

#include <iostream>
#include <sstream>

using namespace std;
using namespace zinc;

int main(int argc, char **argv) {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    string_view url = "https://api.sambanova.ai";
    string_view model = "Meta-Llama-3.1-405B-Instruct";
    string_view key = "d8957211-24e6-426d-90cc-b267ce681e4f";
    key = "b8736160-f724-4b68-bac0-aea866ceee15";
    model = "Llama-3.1-Tulu-3-405B";
    OpenAI client(url, model, key, zinc::span<KeyJSONPair>({{"max_tokens", 4096}}));

    string prompt = "";
    for (int i = 1; i < argc; ++ i) {
        if (i > 1) prompt += " ";
        prompt += argv[i];
    }

    if (prompt.empty()) {
        cerr << "No arguments provided. Consuming stdin." << endl;
        stringstream ss;
        ss << cin.rdbuf();
        prompt = ss.str();
    }

    Log::log(zinc::span<StringViewPair>({
        {"prompt", prompt}
    }));

    cerr << prompt << flush;

    string completion;
    for (auto&& part : client.complete(prompt)) {
        completion += part;
        cout << part << flush;
    }

    Log::log(zinc::span<StringViewPair>({
        {"completion", completion},
    }));

    return 0;
}
