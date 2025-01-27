#pragma once

#include <generator> // C++23 generator support in GCC 14+
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace zinc {

class OpenAI {
public:
    using RoleContentPairs = std::span<std::pair<std::string_view, std::string_view>>;
    using JSONValue = std::variant<std::string_view, double, long, bool>;
    using JSONValues = std::span<std::pair<std::string_view, JSONValue>>;
    struct StreamPart {
        std::string_view text; // Actual generated text
        JSONValues const data; // Raw data returned by the server
        operator std::string_view() const { return text; }
    };

    /**
     * @brief Constructor for initializing the OpenAI client.
     *
     * Common parameter names include:
     * - max_tokens: Maximum number of tokens to generate.
     * - temperature: Controls randomness of predictions.
     * - top_p: Nucleus sampling parameter.
     * - stop: Sequence where the model will stop generating further tokens.
     * - presence_penalty: Penalizes new tokens based on their existing frequency.
     * - frequency_penalty: Penalizes new tokens based on their existing frequency.
     *
     * @param url The base URL of the API endpoint (e.g., "https://api.openai.com").
     * @param model The model name to be used (e.g., "text-davinci-003").
     * @param key The API key for authentication.
     * @param defaults Default parameters for requests.
     */
    OpenAI(
        std::string_view url,
        std::string_view model,
        std::string_view key,
        JSONValues const defaults = {}
    );

    /**
     * @brief Destructor for the OpenAI class.
     */
    ~OpenAI();

    /**
     * @brief Generate a single completion based on a prompt.
     */
    std::generator<StreamPart const&> gen_completion(std::string_view prompt, JSONValues const params = {}) const;

    /**
     * @brief Generate multiple completions based on a prompt.
     *
     * @param completions The number of completions to generate (must be >= 2).
     */
    std::generator<std::span<StreamPart const>> gen_completions(std::string_view prompt, size_t completions, JSONValues const params = {}) const;

    /**
     * @brief Generate a single chat completion based on a series of messages.
     *
     * @param messages A span of pairs representing role-content messages.
     */
    std::generator<StreamPart const&> gen_chat(RoleContentPairs const & messages, JSONValues const params = {}) const;

    /**
     * @brief Generate multiple chat completions based on a series of messages.
     *
     * @param messages A span of pairs representing role-content messages.
     * @param completions The number of completions to generate (must be >= 2).
     */
    std::generator<std::span<StreamPart const>> gen_chats(RoleContentPairs const & messages, size_t completions, JSONValues const params = {}) const;

private:
    std::string const endpoint_completions_;
    std::string const endpoint_chats_;
    std::string const bearer_;
    std::unordered_map<std::string, JSONValue> defaults_;
};

} // namespace zinc
