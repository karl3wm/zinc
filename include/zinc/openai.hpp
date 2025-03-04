#pragma once

#include <zinc/common.hpp>
#include <zinc/json.hpp>

#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace zinc {

class OpenAI {
public:
    using RoleContentPair = StringPair;

    struct StreamPart : public std::string_view {
        JSON data; // Raw data returned by the server
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
     * @param default_params Default parameters for requests.
     */
    OpenAI(
        std::string_view url,
        std::string_view model,
        std::string_view key,
        std::span<KeyJSONPair const> defaults = {}
    );

    /**
     * @brief Destructor for the OpenAI class.
     */
    ~OpenAI();

    /**
     * @brief Stream a completion based on a prompt.
     */
    zinc::generator<StreamPart const&> complete(
        std::string_view prompt,
        std::span<KeyJSONPair const> params = {}
    ) const;

    /**
     * @brief Stream a chat completion based on a series of messages.
     *
     * @param messages A span of pairs representing role-content message context.
     * The first pair element is usually among "system", "user" or "assistant".
     */
    zinc::generator<StreamPart const&> chat(
        std::span<RoleContentPair const> messages,
        std::span<KeyJSONPair const> params = {}
    ) const;

private:
    std::string const endpoint_completions_;
    std::string const endpoint_chats_;
    std::string const bearer_;
    std::vector<std::pair<std::string_view, std::string_view>> headers_;
    std::vector<std::pair<std::string, JSON>> defaults_;
};

} // namespace zinc
