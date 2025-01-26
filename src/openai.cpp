#include <zinc/http.hpp>
#include <zinc/openai.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>

#include <boost/lexical_cast.hpp>
#include <nlohmann/json.hpp>

#include <iostream>

namespace zinc {
using json = nlohmann::json;

// Helper function to validate parameters
static void validate_params(const std::unordered_map<std::string_view, std::string_view>& params, size_t completions = 1) {
    // Verify that the user either does not specify streaming or has stream set to true.
    if (params.find("stream") != params.end() && params.at("stream") != "true") {
        throw std::invalid_argument("Streaming must be enabled for streaming requests.");
    }

    // Verify that the user either does not specify "n" or has "n" equal to 1 for single completions or equal to "completions" for multiple completions.
    if (params.find("n") != params.end()) {
        int n = boost::lexical_cast<int>(params.at("n"));
        if (completions == 1 && n != 1) {
            throw std::invalid_argument("For single completion, 'n' must be 1.");
        } else if (completions > 1 && n != static_cast<int>(completions)) {
            throw std::invalid_argument("For multiple completions, 'n' must match the number of completions.");
        }
    } else if (completions == 0) {
        throw std::invalid_argument("When completions are 0, 'n' must be specified in params.");
    }
}

// Helper function to process response lines
static std::generator<OpenAIClient::CompletionItem const&> process_response_lines(std::generator<std::string_view> & response_lines) {
    for (auto line : response_lines) {
        if (line.empty() || line == "\n") continue; // Skip empty lines

        if (line.front() == '{') { // JSON object
            std::cerr << "process_response_lines line: " << line << std::endl;
            auto json_obj = json::parse(line);
            co_yield OpenAIClient::CompletionItem{json_obj["text"].get<std::string_view>(), json_obj.get<std::unordered_map<std::string_view, std::string_view>>()};
        } else { // Non-JSON informational string
            // TODO: Implement logging or access to these informational strings later.
            // For now, we skip non-JSON lines but log them for debugging purposes.
            // Example: log_info(line);
        }
    }

    co_return;
}

OpenAIClient::OpenAIClient(
    std::string_view url,
    std::string_view model,
    std::string_view key,
    std::span<const std::pair<std::string_view, std::string_view>> defaults)
    : endpoint_completions_(std::string(url) + "/v1/completions"), endpoint_chats_(std::string(url) + "/v1/chat/completions"), bearer_("Bearer " + std::string(key))
{
    defaults_["model"] = model;
    defaults_["stream"] = "true";
    for (const auto& [k, v] : defaults) {
        std::string key(k);
        if (defaults_.find(key) != defaults_.end()) {
            throw std::runtime_error(key + " already specified");
        }
        defaults_[key] = v;
    }
}

OpenAIClient::~OpenAIClient() = default;

std::generator<OpenAIClient::CompletionItem const&> OpenAIClient::gen_completion(std::string_view prompt, std::span<const std::pair<std::string_view, std::string_view>> params) const {
    std::unordered_map<std::string_view, std::string_view> combined_params;
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }
    combined_params["prompt"] = prompt;

    // Validate parameters
    validate_params(combined_params);

    // Build request body
    json j;
    for (const auto& [key, value] : combined_params) {
        j[key] = value;
    }
    std::string body = j.dump();
    

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = HttpClient::request("POST", endpoint_completions_, headers, body);

    // Process response lines
    for (auto& item : process_response_lines(response_lines)) {
        co_yield item;
    }

    co_return;
}

std::generator<std::span<OpenAIClient::CompletionItem const>> OpenAIClient::gen_completions(std::string_view prompt, size_t completions, std::span<const std::pair<std::string_view, std::string_view>> params) const {
    if (completions < 2) {
        throw std::invalid_argument("Number of completions must be at least 2.");
    }

    std::unordered_map<std::string_view, std::string_view> combined_params;
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }
    combined_params["prompt"] = prompt;

    // Validate parameters
    validate_params(combined_params, completions);

    // Build request body
    json j;
    for (const auto& [key, value] : combined_params) {
        j[key] = value;
    }
    std::string body = j.dump();

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = HttpClient::request("POST", endpoint_completions_, headers, body);

    // Process response lines
    auto completion_items = process_response_lines(response_lines);
    std::vector<CompletionItem> items;
    for (auto& item : completion_items) {
        items.push_back(item);
        if (items.size() == completions) {
            co_yield std::span<CompletionItem const>(items.data(), items.size());
            items.clear();
        }
    }

    co_return;
}

std::generator<OpenAIClient::CompletionItem const&> OpenAIClient::gen_chat(std::span<const std::pair<std::string_view, std::string_view>> messages, std::span<const std::pair<std::string_view, std::string_view>> params) const {
    std::unordered_map<std::string_view, std::string_view> combined_params;
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }
    json messages_array = json::array();
    for (const auto& [role, content] : messages) {
        messages_array.push_back({{"role", role}, {"content", content}});
    }
    combined_params["messages"] = messages_array.dump();

    // Validate parameters
    validate_params(combined_params);

    // Build request body
    json j;
    for (const auto& [key, value] : combined_params) {
        j[key] = value;
    }
    std::string body = j.dump();

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = HttpClient::request("POST", endpoint_chats_, headers, body);

    // Process response lines
    auto completions = process_response_lines(response_lines);
    for (auto& item : completions) {
        co_yield item;
    }

    co_return;
}

std::generator<std::span<OpenAIClient::CompletionItem const>> OpenAIClient::gen_chats(std::span<const std::pair<std::string_view, std::string_view>> messages, size_t completions, std::span<const std::pair<std::string_view, std::string_view>> params) const {
    if (completions < 2) {
        throw std::invalid_argument("Number of completions must be at least 2.");
    }

    std::unordered_map<std::string_view, std::string_view> combined_params;
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }
    json messages_array = json::array();
    for (const auto& [role, content] : messages) {
        messages_array.push_back({{"role", role}, {"content", content}});
    }
    combined_params["messages"] = messages_array.dump();

    // Validate parameters
    validate_params(combined_params, completions);

    // Build request body
    json j;
    for (const auto& [key, value] : combined_params) {
        j[key] = value;
    }
    std::string body = j.dump();

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = HttpClient::request("POST", endpoint_chats_, headers, body);

    // Process response lines
    auto completion_items = process_response_lines(response_lines);
    std::vector<CompletionItem> items;
    for (auto& item : completion_items) {
        items.push_back(item);
        if (items.size() == completions) {
            co_yield std::span<CompletionItem const>(items.data(), items.size());
            items.clear();
        }
    }

    co_return;
}

} // namespace zinc
