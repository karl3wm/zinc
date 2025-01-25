#pragma once

#include <string>
#include <string_view>
#include <span>
#include <generator> // C++23 generator support in GCC 14+
#include <map>
#include <vector>

namespace zinc {

class OpenAIClient {
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
     * @param base_url The base URL of the API endpoint (e.g., "https://api.openai.com").
     * @param model The model name to be used (e.g., "text-davinci-003").
     * @param api_key The API key for authentication.
     * @param default_params A span of pairs representing default parameters for requests.
     */
    OpenAIClient(
        std::string_view base_url,
        std::string_view model,
        std::string_view api_key,
        std::span<const std::pair<std::string_view, std::string_view>> default_params = {}
    );

    /**
     * @brief Destructor for the OpenAIClient class.
     */
    ~OpenAIClient();

    /**
     * @brief Generate a single completion based on a prompt.
     *
     * @param prompt The input text prompt.
     * @param params Additional parameters for the request.
     * @return A generator yielding CompletionItem references.
     */
    std::generator<CompletionItem&> gen_completion(std::string_view prompt, const std::map<std::string, std::string>& params = {}) const;

    /**
     * @brief Generate multiple completions based on a prompt.
     *
     * @param prompt The input text prompt.
     * @param completions The number of completions to generate (must be >= 2).
     * @param params Additional parameters for the request.
     * @return A generator yielding spans of CompletionItem objects.
     */
    std::generator<std::span<const CompletionItem>> gen_completions(std::string_view prompt, size_t completions, const std::map<std::string, std::string>& params = {}) const;

    /**
     * @brief Generate a single chat completion based on a series of messages.
     *
     * @param messages A span of pairs representing role-content messages.
     * @param params Additional parameters for the request.
     * @return A generator yielding CompletionItem references.
     */
    std::generator<CompletionItem&> gen_chat(std::span<const std::pair<std::string_view, std::string_view>> messages, const std::map<std::string, std::string>& params = {}) const;

    /**
     * @brief Generate multiple chat completions based on a series of messages.
     *
     * @param messages A span of pairs representing role-content messages.
     * @param completions The number of completions to generate (must be >= 2).
     * @param params Additional parameters for the request.
     * @return A generator yielding spans of CompletionItem objects.
     */
    std::generator<std::span<const CompletionItem>> gen_chats(std::span<const std::pair<std::string_view, std::string_view>> messages, size_t completions, const std::map<std::string, std::string>& params = {}) const;

private:
    struct CompletionItem {
        std::string_view text; // Actual generated text as a view
        std::map<std::string, std::string> metadata; // Metadata returned by the server
        std::span<const std::string_view> additional_data; // Any additional data returned by the server

        // Implicit conversion operator to allow easy use as a string_view
        operator std::string_view() const { return text; }
    };

    // Private member variables
    std::string base_url_;
    std::string model_;
    std::string api_key_;
    std::map<std::string, std::string> default_params_;

    // Helper functions will be implemented in .cpp file
};

} // namespace zinc
