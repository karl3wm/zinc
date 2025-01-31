#include <zinc/openai.hpp>
#include <zinc/log.hpp>

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

    for (int i = 1; i < argc; ++ i) {
        if (i > 1) msg += " ";
        msg += argv[i];
    }

    while ("end of input not reached") {
        std::cerr << std::endl << "user: " << std::flush;
        if (msg.empty()) {
            std::getline(std::cin, msg);
            std::streamsize extra;
            if (!std::cin) {
                break;
            } else while ((extra = std::cin.rdbuf()->in_avail()) > 1) {
                size_t msg_size = msg.size();
                msg.resize(msg_size + extra);
                std::cin.read(&msg[msg_size], extra);
            }
            if (msg.back() == '\n') {
                msg.resize(msg.size() - 1);
            }
        } else {
            std::cerr << msg << std::endl;
        }

        zinc::Log::log({
            {"role", "user"},
            {"content", msg},
        });

        messages.emplace_back("user", std::move(msg));

        std::cerr << std::endl << "assistant: " << std::flush;
        for (auto&& part : client.chat(messages)) {
            msg += part;
            std::cout << part << std::flush;
        }
        std::cout << std::endl;

        zinc::Log::log({
            {"role", "assistant"},
            {"content", msg},
        });

        messages.emplace_back("assistant", std::move(msg));
    }

    return 0;
}
