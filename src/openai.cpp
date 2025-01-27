#include <zinc/http.hpp>
#include <zinc/openai.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>

#include <boost/json.hpp>
#include <boost/json/src.hpp>

#include <iostream>

namespace json = boost::json;

namespace zinc {

// Helper function to validate parameters
static void validate_params(
    std::unordered_map<std::string_view, OpenAI::JSONValue> const & params,
    size_t completions = 1
) { // Verify that the user either does not specify streaming or has stream set to true.
    if (params.find("stream") != params.end() && params.at("stream") != OpenAI::JSONValue(true)) {
        throw std::invalid_argument("Streaming must be enabled for streaming requests.");
    }

    // Verify that the user either does not specify "n" or has "n" equal to 1 for single completions or equal to "completions" for multiple completions.
    if (params.find("n") != params.end()) {
        int n = std::get<long>(params.at("n"));
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
static std::generator<OpenAI::StreamPart const&> process_response_lines(std::generator<std::string_view> & response_lines) {
    static thread_local std::vector<std::pair<std::string_view, OpenAI::JSONValue>> jsonvalues;
    for (auto line : response_lines) {
        if (line.empty() || line == "\n") continue; // Skip empty lines

        if (line.rfind("data: ", 0) == 0) line = line.substr(strlen("data: ")); // SSE prefix

        if (line == "[DONE]") break; // End of stream

        if (line.front() == '{') { // JSON object
            std::cerr << "process_response_lines line: " << line << std::endl;
            json::object json_obj = json::parse(line).get_object();
            jsonvalues.clear();
            for (const auto& [key, value] : json_obj) {
                OpenAI::JSONValue val;
                switch (value.kind()) {
                case json::kind::string:
                    val = value.get_string(); break;
                case json::kind::double_:
                    val = value.get_double(); break;
                case json::kind::int64:
                    val = value.get_int64(); break;
                case json::kind::uint64:
                    val = static_cast<long>(value.get_uint64()); break;
                case json::kind::bool_:
                    val = value.get_bool(); break;
                default:
                    throw std::runtime_error("unexpected json value type");
                }
                jsonvalues.emplace_back(key, val);
            }
            co_yield OpenAI::StreamPart{json_obj["text"].get_string(), jsonvalues};
        } else { // Non-JSON informational string
            // TODO: Implement logging or access to these informational strings later.
            // For now, we skip non-JSON lines but log them for debugging purposes.
            // Example: log_info(line);
            std::cerr << "Non-JSON line: " << line << std::endl;
        }
    }

    co_return;
}

OpenAI::OpenAI(
    std::string_view url,
    std::string_view model,
    std::string_view key,
    JSONValues const defaults)
: endpoint_completions_(std::string(url) + "/v1/completions"),
  endpoint_chats_(std::string(url) + "/v1/chat/completions"),
  bearer_("Bearer " + std::string(key))
{
    defaults_["model"] = model;
    defaults_["stream"] = true;
    for (const auto& [k, v] : defaults) {
        std::string key(k);
        if (defaults_.find(key) != defaults_.end()) {
            throw std::runtime_error(key + " already specified");
        }
        defaults_[key] = v;
    }
}

OpenAI::~OpenAI() = default;

std::generator<OpenAI::StreamPart const&> OpenAI::gen_completion(
    std::string_view prompt,
    JSONValues const params
) const {
    static thread_local std::unordered_map<std::string_view, JSONValue> combined_params;
    combined_params.clear();
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
    json::object j;
    for (const auto& [key, value] : combined_params) {
        std::visit([&](const auto& val) { j[key] = val; }, value);
    }
    std::string body = json::serialize(j);
    

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = Http::request("POST", endpoint_completions_, headers, body);

    // Process response lines
    for (auto& item : process_response_lines(response_lines)) {
        co_yield item;
    }

    co_return;
}

std::generator<std::span<OpenAI::StreamPart const>> OpenAI::gen_completions(
    std::string_view prompt,
    size_t completions,
    JSONValues const params
) const {
    if (completions < 2) {
        throw std::invalid_argument("Number of completions must be at least 2.");
    }

    static thread_local std::unordered_map<std::string_view, JSONValue> combined_params;
    combined_params.clear();
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
    json::object j;
    for (const auto& [key, value] : combined_params) {
        std::visit([&](const auto& val) { j[key] = val; }, value);
    }
    std::string body = json::serialize(j);

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = Http::request("POST", endpoint_completions_, headers, body);

    // Process response lines
    auto completion_items = process_response_lines(response_lines);
    std::vector<StreamPart> items;
    for (auto& item : completion_items) {
        items.push_back(item);
        if (items.size() == completions) {
            co_yield std::span<StreamPart const>(items.data(), items.size());
            items.clear();
        }
    }

    co_return;
}

std::generator<OpenAI::StreamPart const&> OpenAI::gen_chat(
    RoleContentPairs const & messages,
    JSONValues const params
) const {
    static thread_local std::unordered_map<std::string_view, JSONValue> combined_params;
    combined_params.clear();
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }
    json::array messages_array;
    for (const auto& [role, content] : messages) {
        messages_array.push_back({{"role", role}, {"content", content}});
    }

    // Validate parameters
        // hmm validate_params should take a json object to be more flexibly useable after messages_array is stored
    validate_params(combined_params);

    // Build request body
    json::object j;
    for (const auto& [key, value] : combined_params) {
        std::visit([&](const auto& val) { j[key] = val; }, value);
    }
    j["messages"] = messages_array;
    std::string body = json::serialize(j);

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = Http::request("POST", endpoint_chats_, headers, body);

    // Process response lines
    auto completions = process_response_lines(response_lines);
    for (auto& item : completions) {
        co_yield item;
    }

    co_return;
}

std::generator<std::span<OpenAI::StreamPart const>> OpenAI::gen_chats(
    RoleContentPairs const & messages,
    size_t completions,
    JSONValues const params
) const {
    if (completions < 2) {
        throw std::invalid_argument("Number of completions must be at least 2.");
    }

    static thread_local std::unordered_map<std::string_view, JSONValue> combined_params;
    combined_params.clear();
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }
    json::array messages_array;
    for (const auto& [role, content] : messages) {
        messages_array.push_back({{"role", role}, {"content", content}});
    }

    // Validate parameters
    validate_params(combined_params, completions);

    // Build request body
    json::object j;
    for (const auto& [key, value] : combined_params) {
        std::visit([&](const auto& val) { j[key] = val; }, value);
    }
    j["messages"] = messages_array;
    std::string body = json::serialize(j);

    // Perform request
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {
        {"Authorization", bearer_},
        {"Content-Type", "application/json"}
    };
    auto response_lines = Http::request("POST", endpoint_chats_, headers, body);

    // Process response lines
    auto completion_items = process_response_lines(response_lines);
    std::vector<StreamPart> items;
    for (auto& item : completion_items) {
        items.push_back(item);
        if (items.size() == completions) {
            co_yield std::span<StreamPart const>(items.data(), items.size());
            items.clear();
        }
    }

    co_return;
}

} // namespace zinc
