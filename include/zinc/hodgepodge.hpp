#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zinc {

class HodgePodge
{
public:
    struct Message
    {
        std::string role;
        std::optional<std::string> content = {};
        struct ToolCall
        {
            std::string type;
            std::string name;
            std::string parameters;
        };
        std::vector<ToolCall> tool_calls = {};
    };
    static std::string_view prompt_deepseek3(std::vector<Message> messages, bool add_generation_prompt = false);
};

}
