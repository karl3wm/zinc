#pragma once

#include <generator> // C++23 generator support in GCC 14+
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace zinc {

class OpenAI {
public:
    /**
     * @brief Constructor for initializing the OpenAI client.
     *
     * Common parameter names include:
     * - max_tokens: Maximum number of tokens to generate.
     * - temperature: Controls randomness of predictions.
     * - top_p: Nucleus sampling parameter.
     * - n: Number of completions to generate.
     * - stream: Whether to stream results.
     * - stop: Sequence where the model will stop generating further tokens.
     * - presence_penalty: Penalizes new tokens based on their existing frequency.
     * - frequency_penalty: Penalizes new tokens based on their existing frequency.
     *
     * @param url The base URL of the API endpoint (e.g., "https://api.openai.com").
     * @param model The model name to be used (e.g., "text-davinci-003").
     * @param key The API key for authentication.
     * @param defaults A span of pairs representing default parameters for requests.
     */
    OpenAI(
        std::string_view url,
        std::string_view model,
        std::string_view key,
        std::span<std::pair<std::string_view, std::string_view> const> defaults = {}
    );

    /**
     * @brief Destructor for the OpenAI class.
     */
    ~OpenAI();

    struct CompletionItem {
        std::string_view text; // Actual generated text
        std::unordered_map<std::string_view, std::string_view> const data; // Raw data returned by the server
        operator std::string_view() const { return text; }
    };

    /**
     * @brief Generate a single completion based on a prompt.
     */
    std::generator<CompletionItem const&> gen_completion(std::string_view prompt, std::span<std::pair<std::string_view, std::string_view> const> params = {}) const;

    /**
     * @brief Generate multiple completions based on a prompt.
     *
     * @param completions The number of completions to generate (must be >= 2).
     */
    std::generator<std::span<CompletionItem const>> gen_completions(std::string_view prompt, size_t completions,  std::span<std::pair<std::string_view, std::string_view> const> params = {}) const;

    /**
     * @brief Generate a single chat completion based on a series of messages.
     *
     * @param messages A span of pairs representing role-content messages.
     */
    std::generator<CompletionItem const&> gen_chat(std::span<std::pair<std::string_view, std::string_view> const> messages, std::span<std::pair<std::string_view, std::string_view> const> params = {}) const;

    /**
     * @brief Generate multiple chat completions based on a series of messages.
     *
     * @param messages A span of pairs representing role-content messages.
     * @param completions The number of completions to generate (must be >= 2).
     */
    std::generator<std::span<CompletionItem const>> gen_chats(std::span<std::pair<std::string_view, std::string_view> const> messages, size_t completions, std::span<std::pair<std::string_view, std::string_view> const> params = {}) const;

private:
    std::string endpoint_completions_;
    std::string endpoint_chats_;
    std::string bearer_;
    std::unordered_map<std::string, std::string> defaults_;
};

} // namespace zinc
