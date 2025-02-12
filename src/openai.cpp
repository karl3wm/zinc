#include <zinc/http.hpp>
#include <zinc/openai.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include <iostream>

namespace zinc {

// Helper function to validate parameters
static void validate_params(
    std::unordered_map<std::string_view, JSON> const & params,
    size_t completions = 1
) {
    // Verify the the user is not clobbering parameters.

    if (params.find("stream") != params.end() && params.at("stream") != JSON(true)) {
        throw std::invalid_argument("Streaming must be enabled for streaming requests.");
    }

    if (params.find("n") != params.end() && params.at("n") != JSON((long)completions)) {
        throw std::invalid_argument("'n' mismatches the number of completions.");
    }

    if (params.find("prompt") != params.end() || params.find("messages") != params.end()) {
        throw std::invalid_argument("Prompt provided twice.");
    }
}

// Helper function to process response lines
static zinc::generator<std::span<OpenAI::StreamPart>> process_response_lines(zinc::generator<std::string_view> & response_lines) {

    static thread_local std::vector<std::vector<std::pair<std::string_view, JSON>>> jsonvalues_list;
    //static thread_local std::vector<std::unordered_map<std::string_view, OpenAI::JSONValue>> jsonvalues_list;
    static thread_local std::vector<OpenAI::StreamPart> streamparts;

    for (auto line : response_lines) {
        if (line.empty() || line == "\n") continue; // Skip empty lines

        if (line.rfind("data: ", 0) == 0) line = line.substr(strlen("data: ")); // SSE prefix

        if (line == "[DONE]") break; // End of stream

        if (line.front() == '{') { // JSON object
            auto doc = JSON::decode(line);
            JSON::Array choices;
            try {
                choices = (*doc)["choices"].array();
            } catch (std::out_of_range&) {
                if ((*doc)["object"] == JSON("error")) {
                    // got this from targon, could be forwarded from vllm
                    // "{\"message\":\"Failed mid-generation, please retry\",\"object\":\"error\",\"Type\":\"InternalServerError\",\"code\":500}"
                    auto msg = (*doc)["message"].string();
                    if (msg.find("please retry") != decltype(msg)::npos) {
                        throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
                    }
                    throw std::runtime_error(std::string(msg));
                }
            }
            if (choices.size() > jsonvalues_list.size()) {
                jsonvalues_list.resize(choices.size());
            }
            //static std::string dbg;
            //dbg = choices[0]["delta"]["content"].string();
            streamparts.clear();
            for (size_t idx = 0; idx < choices.size(); ++ idx) {
                std::string_view text;
                auto & choice = choices[idx].object();
                auto & jsonvalues = jsonvalues_list[idx];
                jsonvalues.clear();//resize(choice.size());
                //int key_idx = -1;
                for (const auto& [key, value] : choice) {
                    if (value.index() == JSON::STRING && key == "text") {
                        text = value.string();
                    } else if (value.index() == JSON::OBJECT && key == "delta") {
                        text = value["content"].string();
                    }
#if 0
                    JSON val;
                    //++ key_idx;
                    switch (value.index()) {
                    case JSON::STRING:
                        //val = value.get_string();
                        if (key == "text") {
                            text = value.string();//std::get<std::string_view>(val);
                        }
                        break;
                    /*
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
                    //case json::kind::array:
                    //    val = value.get_
                    //    */
                    case JSON::OBJECT:
/*                        if (key == "delta") {
                            val = value.at("content").get_string();
                            //jsonvalues["delta.content"] = val;
                            jsonvalues.emplace_back("delta.content", val);*/
                            //jsonvalues[key_idx].first = "delta.content";
                            //jsonvalues[key_idx].second = val;
                            text = value["content"].string();
                            break;
/*                            continue;
                        } else {
                            throw std::runtime_error("unexpected json value type");
                        }
                        */
                    /*
                    default:
                        throw std::runtime_error("unexpected json value type");
                    */
                    }
                    //jsonvalues[key] = val;
                    jsonvalues.emplace_back(key, val);
                    //jsonvalues[key_idx].first = key;
                    //jsonvalues[key_idx].second = val;
#endif
                }
                /*streamparts.emplace_back(text).data = jsonvalues;*/
                streamparts.emplace_back(text).data = choice;
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
    std::unordered_map<std::string, JSON> defaults_map{
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
    static thread_local std::unordered_map<std::string_view, JSON> combined_params;
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
    static std::vector<KeyJSONPair> paramsvec;
    paramsvec.clear();
    for (const auto& [key, value] : combined_params) {
        paramsvec.emplace_back(key, value);
    }
    paramsvec.emplace_back("prompt", prompt);
    std::string_view body = JSON(paramsvec).encode();

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
    static thread_local std::unordered_map<std::string_view, JSON> combined_params;
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
    static std::vector<KeyJSONPair> paramsvec;
    static std::vector<JSON> messagesvec;
    static std::vector<KeyJSONPair> messagesvec_inner;
    paramsvec.clear();
    messagesvec.clear();
    messagesvec_inner.clear();
    messagesvec_inner.reserve(messages.size() * 2);
    for (const auto& [key, value] : combined_params) {
        paramsvec.emplace_back(key, value);
    }
    for (const auto& [role, content] : messages) {
        auto start = messagesvec_inner.end();
        messagesvec_inner.emplace_back("role", role);
        messagesvec_inner.emplace_back("content", content);
        messagesvec.emplace_back(std::span(start, messagesvec_inner.end()));
    }
    paramsvec.emplace_back("messages", messagesvec);
    std::string_view body = JSON(paramsvec).encode();

    // Perform request
    auto response_lines = HTTP::request_lines("POST", endpoint_chats_, body, headers_);

    // Process response lines
    for (auto const& streamparts : process_response_lines(response_lines)) {
        if (streamparts.size() > 1) throw std::runtime_error("server returned more than 1 completion");
        if (streamparts.size() > 0 && streamparts[0].size() > 0) {
            co_yield streamparts[0];
        }
    }

    co_return;
}

} // namespace zinc
