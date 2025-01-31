#include <zinc/http.hpp>
#include <zinc/openai.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include <boost/json.hpp>
#include <boost/json/src.hpp>

#include <iostream>

namespace json = boost::json;

namespace zinc {

// Helper function to validate parameters
static void validate_params(
    std::unordered_map<std::string_view, OpenAI::JSONValue> const & params,
    size_t completions = 1
) {
    // Verify the the user is not clobbering parameters.

    if (params.find("stream") != params.end() && params.at("stream") != OpenAI::JSONValue(true)) {
        throw std::invalid_argument("Streaming must be enabled for streaming requests.");
    }

    if (params.find("n") != params.end() && params.at("n") != OpenAI::JSONValue((long)completions)) {
        throw std::invalid_argument("'n' mismatches the number of completions.");
    }

    if (params.find("prompt") != params.end() || params.find("messages") != params.end()) {
        throw std::invalid_argument("Prompt provided twice.");
    }
}

// Helper function to process response lines
static zinc::generator<std::span<OpenAI::StreamPart>> process_response_lines(zinc::generator<std::string_view> & response_lines) {

    static thread_local std::vector<std::vector<std::pair<std::string_view, OpenAI::JSONValue>>> jsonvalues_list;
    //static thread_local std::vector<std::unordered_map<std::string_view, OpenAI::JSONValue>> jsonvalues_list;
    static thread_local std::vector<OpenAI::StreamPart> streamparts;

    for (auto line : response_lines) {
        if (line.empty() || line == "\n") continue; // Skip empty lines

        if (line.rfind("data: ", 0) == 0) line = line.substr(strlen("data: ")); // SSE prefix

        if (line == "[DONE]") break; // End of stream

        if (line.front() == '{') { // JSON object
            json::array choices = json::parse(line).at("choices").get_array();
            if (choices.size() > jsonvalues_list.size()) {
                jsonvalues_list.resize(choices.size());
            }
            streamparts.clear();
            for (size_t idx = 0; idx < choices.size(); ++ idx) {
                std::string_view text;
                json::object & choice = choices[idx].get_object();
                auto & jsonvalues = jsonvalues_list[idx];
                jsonvalues.clear();//resize(choice.size());
                //int key_idx = -1;
                for (const auto& [key, value] : choice) {
                    OpenAI::JSONValue val;
                    //++ key_idx;
                    switch (value.kind()) {
                    case json::kind::string:
                        val = value.get_string();
                        if (key == "text") {
                            text = std::get<std::string_view>(val);
                        }
                        break;
                    case json::kind::double_:
                        val = value.get_double(); break;
                    case json::kind::int64:
                        val = value.get_int64(); break;
                    case json::kind::uint64:
                        val = static_cast<long>(value.get_uint64()); break;
                    case json::kind::bool_:
                        val = value.get_bool(); break;
                    case json::kind::null:
                        val = nullptr; break;
                    case json::kind::object:
                        if (key == "delta") {
                            val = value.at("content").get_string();
                            //jsonvalues["delta.content"] = val;
                            jsonvalues.emplace_back("delta.content", val);
                            //jsonvalues[key_idx].first = "delta.content";
                            //jsonvalues[key_idx].second = val;
                            text = std::get<std::string_view>(val);
                            continue;
                        }
                        // fall-thru
                    default:
                        throw std::runtime_error("unexpected json value type");
                    }
                    //jsonvalues[key] = val;
                    jsonvalues.emplace_back(key, val);
                    //jsonvalues[key_idx].first = key;
                    //jsonvalues[key_idx].second = val;
                }
                streamparts.emplace_back(text, jsonvalues);
            }

            co_yield streamparts;

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
    std::span<KeyJSONPair const> defaults)
: endpoint_completions_(std::string(url) + "/v1/completions"),
  endpoint_chats_(std::string(url) + "/v1/chat/completions"),
  bearer_("Bearer " + std::string(key)),
  headers_{
    {"Authorization", bearer_},
    {"Content-Type", "application/json"},
    {"HTTP-Referer", "https://github.com/karl3wm/zinc"},
    {"X-Title", "ZinC"}
  }
{
    std::unordered_map<std::string, JSONValue> defaults_map{
        {"model", model},
        {"stream", true}
    };
    for (const auto& [k, v] : defaults) {
        std::string key(k);
        if (defaults_map.find(key) != defaults_map.end()) {
            throw std::runtime_error(key + " already specified");
        }
        defaults_map[key] = v;
    }
    defaults_ = decltype(defaults_)(defaults_map.begin(), defaults_map.end());
}

OpenAI::~OpenAI() = default;

zinc::generator<OpenAI::StreamPart const&> OpenAI::complete(
    std::string_view prompt,
    std::span<KeyJSONPair const> params
) const {
    static thread_local std::unordered_map<std::string_view, JSONValue> combined_params;
    combined_params.clear();
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }

    // Validate parameters
    validate_params(combined_params);

    // Build request body
    json::object j;
    for (const auto& [key, value] : combined_params) {
        std::visit([&,key=key](const auto& val) { j[key] = val; }, value);
    }
    j["prompt"] = prompt;
    std::string body = json::serialize(j);
    

    // Perform request
    auto response_lines = HTTP::request_lines("POST", endpoint_completions_, body, headers_);

    // Process response lines
    for (auto const& streamparts : process_response_lines(response_lines)) {
        if (streamparts.size() > 1) throw std::runtime_error("server returned more than 1 completion");
        if (streamparts.size() > 0) co_yield streamparts[0];
    }

    co_return;
}

zinc::generator<OpenAI::StreamPart const&> OpenAI::chat(
    std::span<RoleContentPair const> messages,
    std::span<KeyJSONPair const> params
) const {
    static thread_local std::unordered_map<std::string_view, JSONValue> combined_params;
    combined_params.clear();
    for (const auto& [k, v] : defaults_) {
        combined_params[k] = v;
    }
    for (const auto& [k, v] : params) {
        combined_params[k] = v;
    }

    // Validate parameters
        // hmm validate_params should take a json object to be more flexibly useable after messages_array is stored
    validate_params(combined_params);

    // Build request body
    json::object j;
    for (const auto& [key, value] : combined_params) {
        std::visit([&,key=key](const auto& val) { j[key] = val; }, value);
    }
    json::array messages_array;
    for (const auto& [role, content] : messages) {
        messages_array.push_back({{"role", role}, {"content", content}});
    }
    j["messages"] = messages_array;
    std::string body = json::serialize(j);

    // Perform request
    auto response_lines = HTTP::request_lines("POST", endpoint_chats_, body, headers_);

    // Process response lines
    for (auto const& streamparts : process_response_lines(response_lines)) {
        if (streamparts.size() > 1) throw std::runtime_error("server returned more than 1 completion");
        if (streamparts.size() > 0) co_yield streamparts[0];
    }

    co_return;
}

} // namespace zinc
