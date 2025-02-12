#include <zinc/hodgepodge.hpp>

#include <sstream>

//char const*deepseek3 =
std::string_view zinc::HodgePodge::prompt_deepseek3(
    std::vector<Message> messages,
    //"{% if not add_generation_prompt is defined %}"
    //    "{% set add_generation_prompt = false %}"
    //"{% endif %}"
    bool add_generation_prompt
)
{
    static thread_local std::stringstream result;
    result.str({});
    //"{% set ns = namespace(is_first=false, is_tool=false, is_output_first=true, system_prompt='', is_first_sp=true) %}"
    bool is_first = false;
    bool is_tool = false;
    bool is_output_first = true;
    std::string system_prompt = "";
    bool is_first_sp = true;
    //"{%- for message in messages %}"
    for (auto & message : messages) {
        //"{%- if message['role'] == 'system' %}"
        if ("system" == message.role) {
            //"{%- if ns.is_first_sp %}"
            if (is_first_sp) {
                //"{% set ns.system_prompt = ns.system_prompt + message['content'] %}"
                system_prompt += message.content.value();
                //"{% set ns.is_first_sp = false %}"
                is_first_sp = false;
            //"{%- else %}"
            } else {
                //"{% set ns.system_prompt = ns.system_prompt + '\n\n' + message['content'] %}"
                system_prompt += "\n\n" + message.content.value();
            //"{%- endif %}"
            }
        //"{%- endif %}"
        }
    //"{%- endfor %}"
    }
    //"{{bos_token}}{{ns.system_prompt}}"
    result << "<｜begin▁of▁sentence｜>" << system_prompt;
    //"{%- for message in messages %}"
    for (auto & message : messages) {
        //"{%- if message['role'] == 'user' %}"
        if ("user" == message.role) {
            //"{%- set ns.is_tool = false -%}"
            is_tool = false;
            //"{{'<｜User｜>' + message['content']}}"
            result << "<｜User｜>" + message.content.value();
        //"{%- endif %}"
        }
        //"{%- if message['role'] == 'assistant' and message['content'] is none %}"
        if ("assistant" == message.role && !message.content.has_value()) {
            //"{%- set ns.is_tool = false -%}"
            is_tool = false;
            //"{%- for tool in message['tool_calls']%}"
            for (auto & tool : message.tool_calls) {
                //"{%- if not ns.is_first %}"
                if (! is_first) {
                    //"{{'<｜Assistant｜><｜tool▁calls▁begin｜><｜tool▁call▁begin｜>' + tool['type'] + '<｜tool▁sep｜>' + tool['function']['name'] + '\n' + '```json' + '\n' + tool['function']['arguments'] + '\n' + '```' + '<｜tool▁call▁end｜>'}}"
                    result << "<｜Assistant｜><｜tool▁calls▁begin｜><｜tool▁call▁begin｜>" + tool.type + "<｜tool▁sep｜>" + tool.name + "\n" + "```json" + "\n" + tool.parameters + "\n" + "```" + "<｜tool▁call▁end｜>";
                    //"{%- set ns.is_first = true -%}"
                    is_first = false;
                //"{%- else %}"
                } else {
                    //"{{'\n' + '<｜tool▁call▁begin｜>' + tool['type'] + '<｜tool▁sep｜>' + tool['function']['name'] + '\n' + '```json' + '\n' + tool['function']['arguments'] + '\n' + '```' + '<｜tool▁call▁end｜>'}}"
                    result << "\n" "<｜tool▁call▁begin｜>" + tool.type + "<｜tool▁sep｜>" + tool.name + "\n" "```json" "\n" + tool.parameters + "\n" "```" "<｜tool▁call▁end｜>";
                    //"{{'<｜tool▁calls▁end｜><｜end▁of▁sentence｜>'}}"
                    result << "<｜tool▁calls▁end｜><｜end▁of▁sentence｜>";
                //"{%- endif %}"
                }
            //"{%- endfor %}"
            }
        //"{%- endif %}"
        }
        //"{%- if message['role'] == 'assistant' and message['content'] is not none %}"
        if ("assistant" == message.role && message.content.has_value()) {
            //"{%- if ns.is_tool %}"
            if (is_tool) {
                //"{{'<｜tool▁outputs▁end｜>' + message['content'] + '<｜end▁of▁sentence｜>'}}"
                result << "<｜tool▁outputs▁end｜>" + message.content.value() + "<｜end▁of▁sentence｜>";
                //"{%- set ns.is_tool = false -%}"
                is_tool = false;
            //"{%- else %}"
            } else {
                //"{{'<｜Assistant｜>' + message['content'] + '<｜end▁of▁sentence｜>'}}"
                result << "<｜Assistant｜>" + message.content.value() + "<｜end▁of▁sentence｜>";
            //"{%- endif %}"
            }
        //"{%- endif %}"
        }
        //"{%- if message['role'] == 'tool' %}"
        if ("tool" == message.role) {
            //"{%- set ns.is_tool = true -%}"
            is_tool = true;
            //"{%- if ns.is_output_first %}"
            if (is_output_first) {
                //"{{'<｜tool▁outputs▁begin｜><｜tool▁output▁begin｜>' + message['content'] + '<｜tool▁output▁end｜>'}}"
                result << "<｜tool▁outputs▁begin｜><｜tool▁output▁begin｜>" + message.content.value() + "<｜tool▁output▁end｜>";
                //"{%- set ns.is_output_first = false %}"
                is_output_first = false;
            //"{%- else %}"
            } else {
                //"{{'\n<｜tool▁output▁begin｜>' + message['content'] + '<｜tool▁output▁end｜>'}}"
                result << "\n<｜tool▁output▁begin｜>" + message.content.value() + "<｜tool▁output▁end｜>";
            //"{%- endif %}"
            }
        //"{%- endif %}"
        }
    //"{%- endfor -%}"
    }
    //"{% if ns.is_tool %}"
    if (is_tool) {
        //"{{'<｜tool▁outputs▁end｜>'}}"
        result << "<｜tool▁outputs▁end｜>";
    //"{% endif %}"
    }
    //"{% if add_generation_prompt and not ns.is_tool %}"
    if (add_generation_prompt && !is_tool) {
        //"{{'<｜Assistant｜>'}}"
        result << "<｜Assistant｜>";
    }
    //"{% endif %}";
    return result.view();
}
