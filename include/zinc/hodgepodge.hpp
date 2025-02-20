#pragma once

#include <zinc/common.hpp>

#include <zinc/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace zinc {

class HodgePodge
{
public:
    struct ToolDefinition
    {
        std::string tool_name;
        std::string description;
        struct ToolParamDefinition
        {
            std::string name;
            std::string param_type;
            std::string description;
            bool required;
        };
        std::vector<ToolParamDefinition> parameters;
    };
    struct Message
    {
        std::string role;
        std::optional<std::string> content = {};
        struct ToolCall
        {
            std::string type;
            struct Function {
                std::string name;
                zinc::JSON parameters;
            } function;
        };
        std::vector<ToolCall> tool_calls = {};
    };
    /*
    struct Tokenization
    {
        static std::string_view(*prompt_template)(std::vector<Message> messages, bool add_generation_prompt = false);
        static std::string_view bos_token, eos_token, pad_token;
        static size_t bos_token_id, eos_token, pad_token;
    };
    */
    static std::string_view prompt_llama31_hf(
        std::vector<Message> messages,
        bool add_generation_prompt = false,
        std::vector<std::string_view> builtin_tools = {"code_interpreter", "brave_search", "wolfram_alpha"},
        std::vector<ToolDefinition> custom_tools = {},
        bool tools_in_user_message = true,
        std::string_view date_string = "26 Jul 2024"
    );
    static std::string_view prompt_deepseek3(
        std::vector<Message> messages,
        bool add_generation_prompt = false
    );
};

}
