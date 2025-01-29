#include <zinc/openai.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

std::string escaped(std::string_view str)
{
    return zinc::replaced(
        str,
        zinc::span<zinc::StringViewPair>({
            {"\\","\\\\"},
            {"\"","\\\""},
            {"\n","\\n"}
        })
    );
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char **argv) {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    std::string_view url = "https://api.sambanova.ai";
    std::string_view model = "Meta-Llama-3.1-405B-Instruct";
    std::string_view key = "d8957211-24e6-426d-90cc-b267ce681e4f";
    zinc::OpenAI client(url, model, key);

    // Get the current date and time
    auto now = std::chrono::system_clock::now();
    // Format the time in ISO 8601
    // Create the logfile
    std::ofstream logf(std::format("{:%FT%TZ}.log",now));

    std::vector<zinc::OpenAI::RoleContentPair> messages;
    std::string msg;

    for (int i = 1; i < argc; ++ i) {
        if (i > 1) msg += " ";
        msg += argv[i];
    }

    if (!logf.is_open()) {
        throw std::runtime_error("Failed to write file.");
    }
    logf << std::fixed << std::setprecision(3);

    while ("end of input not reached") {
        std::cerr << "user: " << std::flush;
        if (msg.empty()) {
            std::getline(std::cin, msg);
            if (!std::cin) {
                break;
            }
        }

        // Get the current date and time
        now = std::chrono::system_clock::now();
        // Convert to milliseconds since epoch
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        logf << "{\"role\":\"user\",\"content\":\"" << escaped(msg) << "\",\"ts\":" << (now_ms/1000.0l) << "}" << std::endl;

        messages.emplace_back("user", std::move(msg));
        // be nice to read more lines if there is content in cin's buffer
        // this would handle accidental pastes, for example

        std::cerr << "assistant: " << std::flush;
        for (auto&& part : client.chat(messages)) {
            msg += part;
            std::cout << part << std::flush;
        }
        std::cout << std::endl;

        // Get the current date and time
        // Convert to milliseconds since epoch
        now = std::chrono::system_clock::now();
        now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        logf << "{\"role\":\"assistant\",\"content\":\"" << escaped(msg) << "\",\"ts\":" << (now_ms/1000.0l) << "}" << std::endl;

        messages.emplace_back("assistant", std::move(msg));
    }

    return 0;
}
