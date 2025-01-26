#include <zinc/http.hhp>
#include <zinc/openai.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace zinc {

// Helper function to validate parameters
static void validate_params(const std::unordered_map<std::string, std::string>& params, size_t completions = 1) {
    // Verify that the user either does not specify streaming or has stream set to true.
    if (params.find("stream") != params.end() && params.at("stream") != "true") {
        throw std::invalid_argument("Streaming must be enabled for streaming requests.");
    }

    // Verify that the user either does not specify "n" or has "n" equal to 1 for single completions or equal to "completions" for multiple completions.
    if (params.find("n") != params.end()) {
        int n = std::stoi(params.at("n"));
        if (completions == 1 && n != 1) {
            throw std::invalid_argument("For single completion, 'n' must be 1.");
        } else if (completions > 1 && n != static_cast<int>(completions)) {
            throw std::invalid_argument("For multiple completions, 'n' must match the number of completions.");
        }
    } else if (completions == 0) {
        throw std::invalid_argument("When completions are 0, 'n' must be specified in params.");
    }
}

// Helper function to build request headers
static std::vector<std::pair<std::string_view, std::string_view>> build_headers(const std::string& api_key) {
    return {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type", "application/json"}
    };
}

// Helper function to process response lines
static std::generator<CompletionItem> process_response_lines(std::generator<std::string_view> response_lines) {
    for (auto line : response_lines) {
        if (line.empty() || line == "\n") continue; // Skip empty lines

        if (line.front() == '{') { // JSON object
            auto json_obj = parse_json(line); // Assume a function exists to parse JSON into a map
            yield CompletionItem{json_obj["text"], json_obj};
        } else { // Non-JSON informational string
            // TODO: Implement logging or access to these informational strings later.
            // For now, we skip non-JSON lines but log them for debugging purposes.
            // Example: log_info(line);
        }
    }
}

OpenAIClient::OpenAIClient(
    std::string_view url,
    std::string_view model,
    std::string_view key,
    std::span<const std::pair<std::string_view, std::string_view>> defaults)
    : base_url_(std::string(url)), model_(std::string(model)), api_key_(std::string(key))
{
    for (const auto& [k, v] : defaults) {
        defaults_[std::string(k)] = std::string(v);
    }
}

OpenAIClient::~OpenAIClient() = default;

std::generator<CompletionItem const&> OpenAIClient::gen_completion(std::string_view prompt, std::span<const std::pair<std::string, std::string>> params) const {
    std::unordered_map<std::string, std::string> combined_params = defaults_;
    for (const auto& [k, v] : params) {
        combined_params[std::string(k)] = v;
    }
    combined_params["prompt"] = std::string(prompt);

    // Validate parameters
    validate_params(combined_params);

    // Build request body
    std::string body = build_request_body(combined_params); // Assume a function exists to build JSON body from map

    // Perform request
    auto headers = build_headers(api_key_);
    auto response_lines = HttpClient::request("POST", base_url_ + "/v1/completions", headers, body);

    // Process response lines
    auto completions = process_response_lines(response_lines);
    for (auto& item : completions) {
        co_yield item;
    }
}

std::generator<std::span<CompletionItem const>> OpenAIClient::gen_completions(std::string_view prompt, size_t completions, std::span<const std::pair<std::string, std::string>> params) const {
    if (completions < 2) {
        throw std::invalid_argument("Number of completions must be at least 2.");
    }

    std::unordered_map<std::string, std::string> combined_params = defaults_;
    for (const auto& [k, v] : params) {
        combined_params[std::string(k)] = v;
    }
    combined_params["prompt"] = std::string(prompt);

    // Validate parameters
    validate_params(combined_params, completions);

    // Build request body
    std::string body = build_request_body(combined_params); // Assume a function exists to build JSON body from map

    // Perform request
    auto headers = build_headers(api_key_);
    auto response_lines = HttpClient::request("POST", base_url_ + "/v1/completions", headers, body);

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
}

std::generator<CompletionItem const&> OpenAIClient::gen_chat(std::span<const std::pair<std::string_view, std::string_view>> messages, std::span<const std::pair<std::string, std::string>> params) const {
    std::unordered_map<std::string, std::string> combined_params = defaults_;
    for (const auto& [k, v] : params) {
        combined_params[std::string(k)] = v;
    }
    combined_params["messages"] = build_messages_param(messages); // Assume a function exists to build messages param from span

    // Validate parameters
    validate_params(combined_params);

    // Build request body
    std::string body = build_request_body(combined_params); // Assume a function exists to build JSON body from map

    // Perform request
    auto headers = build_headers(api_key_);
    auto response_lines = HttpClient::request("POST", base_url_ + "/v1/chat/completions", headers, body);

    // Process response lines
    auto completions = process_response_lines(response_lines);
    for (auto& item : completions) {
        co_yield item;
    }
}

std::generator<std::span<CompletionItem const>> OpenAIClient::gen_chats(std::span<const std::pair<std::string_view, std::string_view>> messages, size_t completions, std::span<const std::pair<std::string, std::string>> params) const {
    if (completions < 2) {
        throw std::invalid_argument("Number of completions must be at least 2.");
    }

    std::unordered_map<std::string, std::string> combined_params = defaults_;
    for (const auto& [k, v] : params) {
        combined_params[std::string(k)] = v;
    }
    combined_params["messages"] = build_messages_param(messages); // Assume a function exists to build messages param from span

    // Validate parameters
    validate_params(combined_params, completions);

    // Build request body
    std::string body = build_request_body(combined_params); // Assume a function exists to build JSON body from map

    // Perform request
    auto headers = build_headers(api_key_);
    auto response_lines = HttpClient::request("POST", base_url_ + "/v1/chat/completions", headers, body);

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
}

} // namespace zinc
