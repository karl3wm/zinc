#include <zinc/hodgepodge.hpp>

#include <iomanip>
#include <sstream>

//char const*llama31 =
std::string_view zinc::HodgePodge::prompt_llama31_hf(
    std::vector<Message> messages,
    bool add_generation_prompt,
    //"{%- if custom_tools is defined %}\n"
    //"    {%- set tools = custom_tools %}\n"
    //"{%- endif %}\n"
    //"{%- if not tools_in_user_message is defined %}\n"
    //"    {%- set tools_in_user_message = true %}\n"
    //"{%- endif %}\n"
    //"{%- if not date_string is defined %}\n"
    //"    {%- set date_string = \"26 Jul 2024\" %}\n"
    //"{%- endif %}\n"
    //"{%- if not tools is defined %}\n"
    //"    {%- set tools = none %}\n"
    //"{%- endif %}\n"
    std::vector<std::string_view> builtin_tools,
    std::vector<ToolDefinition> custom_tools,
    bool tools_in_user_message,
    std::string_view date_string
)
// default system message is "You are a helpful assistant."
{
    auto & tools = custom_tools;
    static thread_local std::stringstream result;
    std::string_view system_message;
    std::string_view first_user_message;
    result.str({});

    auto insert_tojson = [](auto & result, auto & t) {
        result << "{\n";
        result << "    \"name\": " << std::quoted(t.tool_name) << ",\n";
        result << "    \"description\": " << std::quoted(t.description) << ",\n";
        result << "    \"parameters\": {\n";
        bool first = true;
        for (auto & p : t.parameters) {
        if (first) {
            first = false;
        } else {
            result      << ",\n";
        }
        result << "        \"" << p.name << "\": {\n";
        result << "            \"param_type\": " << std::quoted(p.param_type) << ",\n";
        result << "            \"description\": " << std::quoted(p.description) << ",\n";
        result << "            \"required\": " << (p.required?"true":"false") << "\n";
        result << "        }";
        }
        result          << "\n";
        result << "    }\n";
        result << "}";
    };

    //"{{- bos_token }}\n"
    result << "<|begin_of_text|>";
    //"\n"
    //"{#- This block extracts the system message, so we can slot it into the right place. #}\n"
    //"{%- if messages[0]['role'] == 'system' %}\n"
    if (messages[0].role == "system") {
    //"    {%- set system_message = messages[0]['content']|trim %}\n"
        system_message = zinc::trim(*messages[0].content);
    //"    {%- set messages = messages[1:] %}\n"
        messages = decltype(messages)(messages.begin() + 1, messages.end());
    //"{%- else %}\n"
    } else {
    //"    {%- set system_message = \"\" %}\n"
        system_message = "";
    //"{%- endif %}\n"
    }
    //"\n"
    //"{#- System message + builtin tools #}\n"
    //"{{- \"<|start_header_id|>system<|end_header_id|>\\n\\n\" }}\n"
    result << "<|start_header_id|>system<|end_header_id|>\n\n";
    //"{%- if builtin_tools is defined or tools is not none %}\n"
    if (builtin_tools.size() || tools.size()) {
    //"    {{- \"Environment: ipython\\n\" }}\n"
        result << "Environment: ipython\n";
    //"{%- endif %}\n"
    }
    //"{%- if builtin_tools is defined %}\n"
    if (builtin_tools.size()) {
    //"    {{- \"Tools: \" + builtin_tools | reject('equalto', 'code_interpreter') | join(\", \") + \"\\n\\n\"}}\n"
        result << "Tools: ";
        bool first = true;
        for (auto & tool : builtin_tools) {
            if (tool == "code_interpreter") continue;
            if (first) first = false; else result << ", ";
            result << tool;
        }
        result << "\n\n";
    //"{%- endif %}\n"
    }
    //"{{- \"Cutting Knowledge Date: December 2023\\n\" }}\n"
    result << "Cutting Knowledge Date: December 2023\n";
    //"{{- \"Today Date: \" + date_string + \"\\n\\n\" }}\n"
    result << "Today Date: " << date_string << "\n\n";
    //"{%- if tools is not none and not tools_in_user_message %}\n"
    if (!tools.size() && !tools_in_user_message) {
    //"    {{- \"You have access to the following functions. To call a function, please respond with JSON for a function call.\" }}\n"
        result << "You have access to the following functions. To call a function, please respond with JSON for a function call.";
    //"    {{- 'Respond in the format {\"name\": function name, \"parameters\": dictionary of argument name and its value}.' }}\n"
        result << "Respond in the format {\"name\": function name, \"parameters\": dictionary of argument name and its value}.";
    //"    {{- \"Do not use variables.\\n\\n\" }}\n"
        result << "Do not use variables.\n\n";
    //"    {%- for t in tools %}\n"
        for (auto & t : tools) {
    //"        {{- t | tojson(indent=4) }}\n"
            insert_tojson(result, t);
    //"        {{- \"\\n\\n\" }}\n"
            result << "\n\n";
    //"    {%- endfor %}\n"
        }
    //"{%- endif %}\n"
    }
    //"{{- system_message }}\n"
    result << system_message;
    //"{{- \"<|eot_id|>\" }}\n"
    result << "<|eot_id|>";
    //"\n"
    //"{#- Custom tools are passed in a user message with some extra guidance #}\n"
    //"{%- if tools_in_user_message and not tools is none %}\n"
    if (tools_in_user_message && tools.size()) {
    //"    {#- Extract the first user message so we can plug it in here #}\n"
    //"    {%- if messages | length != 0 %}\n"
        if (messages.size() != 0) {
    //"        {%- set first_user_message = messages[0]['content']|trim %}\n"
            first_user_message = zinc::trim(*messages[0].content);
    //"        {%- set messages = messages[1:] %}\n"
            messages = decltype(messages)(messages.begin() + 1, messages.end());
    //"    {%- else %}\n"
        } else {
    //"        {{- raise_exception(\"Cannot put tools in the first user message when there's no first user message!\") }}\n"
            throw std::runtime_error("Cannot put tools in the first user message when there's no first user message!");
    //"{%- endif %}\n"
        }
    //"    {{- '<|start_header_id|>user<|end_header_id|>\\n\\n' -}}\n"
        result << "<|start_header_id|>user<|end_header_id|>\n\n";
    //"    {{- \"Given the following functions, please respond with a JSON for a function call \" }}\n"
        result << "Given the following functions, please respond with a JSON for a function call ";
    //"    {{- \"with its proper arguments that best answers the given prompt.\\n\\n\" }}\n"
        result << "with its proper arguments that best answers the given prompt.\n\n";
    //"    {{- 'Respond in the format {\"name\": function name, \"parameters\": dictionary of argument name and its value}.' }}\n"
        result << "Respond in the format {\"name\": function name, \"parameters\": dictionary of argument name and its value}.";
    //"    {{- \"Do not use variables.\\n\\n\" }}\n"
        result << "Do not use variables.\n\n";
    //"    {%- for t in tools %}\n"
        for (auto & t : tools) {
    //"        {{- t | tojson(indent=4) }}\n"
            insert_tojson(result, t);
    //"        {{- \"\\n\\n\" }}\n"
            result << "\n\n";
    //"    {%- endfor %}\n"
        }
    //"    {{- first_user_message + \"<|eot_id|>\"}}\n"
        result << first_user_message << "<|eot_id|>";
    //"{%- endif %}\n"
    }
    //"\n"
    //"{%- for message in messages %}\n"
    for (auto & message : messages) {
    //"    {%- if not (message.role == 'ipython' or message.role == 'tool' or 'tool_calls' in message) %}\n"
        if ( ! (message.role == "ipython" || message.role == "tool" || message.tool_calls.size()) ) {
    //"        {{- '<|start_header_id|>' + message['role'] + '<|end_header_id|>\\n\\n'+ message['content'] | trim + '<|eot_id|>' }}\n"
            result << "<|start_header_id|>" << message.role << "<|end_header_id|>\n\n" << zinc::trim(*message.content) << "<|eot_id|>";
    //"    {%- elif 'tool_calls' in message %}\n"
        } else if (message.tool_calls.size()) {
    //"        {%- if not message.tool_calls|length == 1 %}\n"
            if (message.tool_calls.size() != 1) {
    //"            {{- raise_exception(\"This model only supports single tool-calls at once!\") }}\n"
                throw std::runtime_error("This model only supports single tool-calls at once!");
    //"        {%- endif %}\n"
            }
    //"        {%- set tool_call = message.tool_calls[0].function %}\n"
            auto tool_call = message.tool_calls[0].function;
    //"        {%- if builtin_tools is defined and tool_call.name in builtin_tools %}\n"
            bool tool_call_is_builtin = false;
            for (auto & t : builtin_tools)
                if (tool_call.name == t)
                    tool_call_is_builtin = true;
            if (tool_call_is_builtin) {
    //"            {{- '<|start_header_id|>assistant<|end_header_id|>\\n\\n' -}}\n"
                result << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    //"            {{- \"<|python_tag|>\" + tool_call.name + \".call(\" }}\n"
                result << "<|python_tag|>" << tool_call.name << ".call(";
    //"            {%- for arg_name, arg_val in tool_call.arguments | items %}\n"
                size_t first = true;
                for (auto & [arg_name, arg_val] : tool_call.parameters.object()) {
    //"                {{- arg_name + '=\"' + arg_val + '\"' }}\n"
                    result << arg_name << "=\"" << arg_val.string() << "\"";
    //"                {%- if not loop.last %}\n"
    //"                    {{- \", \" }}\n"
    //"                {%- endif %}\n"
                    if (first) first = false; else result << ", ";
    //"                {%- endfor %}\n"
                }
    //"            {{- \")\" }}\n"
                result << ")";
    //"        {%- else  %}\n"
            } else {
    //"            {{- '<|start_header_id|>assistant<|end_header_id|>\\n\\n' -}}\n"
                result << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    //"            {{- '{\"name\": \"' + tool_call.name + '\", ' }}\n"
                result << "{\"name\": \"" + tool_call.name + "\", ";
    //"            {{- '\"parameters\": ' }}\n"
                result << "\"parameters\": ";
    //"            {{- tool_call.arguments | tojson }}\n"
                result << tool_call.parameters.encode();
    //"            {{- \"}\" }}\n"
                result << "}";
    //"        {%- endif %}\n"
            };
    //"        {%- if builtin_tools is defined %}\n"
            if (builtin_tools.size()) {
    //"            {#- This means we're in ipython mode #}\n"
    //"            {{- \"<|eom_id|>\" }}\n"
                result << "<|eom_id|>";
    //"        {%- else %}\n"
            } else {
    //"            {{- \"<|eot_id|>\" }}\n"
                result << "<|eot_id|>";
    //"        {%- endif %}\n"
            }
    //"    {%- elif message.role == \"tool\" or message.role == \"ipython\" %}\n"
        } else if (message.role == "tool" || message.role == "ipython") {
    //"        {{- \"<|start_header_id|>ipython<|end_header_id|>\\n\\n\" }}\n"
            result << "<|start_header_id|>ipython<|end_header_id|>\n\n";
    //"        {%- if message.content is mapping or message.content is iterable %}\n"
    //"            {{- message.content | tojson }}\n"
    //"        {%- else %}\n"
    //"            {{- message.content }}\n"
    //"        {%- endif %}\n"
            result << message.content.value();
    //"        {{- \"<|eot_id|>\" }}\n"
            result << "<|eot_id|>";
    //"    {%- endif %}\n"
        }
    //"{%- endfor %}\n"
    }
    //"{%- if add_generation_prompt %}\n"
    if (add_generation_prompt) {
    //"    {{- '<|start_header_id|>assistant<|end_header_id|>\\n\\n' }}\n"
        result << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    //"{%- endif %}\n";
    }
    return result.view();
}

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
    static thread_local std::string system_prompt;
    system_prompt = "";
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
                    result << "<｜Assistant｜><｜tool▁calls▁begin｜><｜tool▁call▁begin｜>" << tool.type << "<｜tool▁sep｜>" << tool.function.name << "\n" << "```json" << "\n" << tool.function.parameters.encode() << "\n" << "```" << "<｜tool▁call▁end｜>";
                    //"{%- set ns.is_first = true -%}"
                    is_first = false;
                //"{%- else %}"
                } else {
                    //"{{'\n' + '<｜tool▁call▁begin｜>' + tool['type'] + '<｜tool▁sep｜>' + tool['function']['name'] + '\n' + '```json' + '\n' + tool['function']['arguments'] + '\n' + '```' + '<｜tool▁call▁end｜>'}}"
                    result << "\n" "<｜tool▁call▁begin｜>" + tool.type + "<｜tool▁sep｜>" << tool.function.name << "\n" "```json" "\n" << tool.function.parameters.encode() << "\n" "```" "<｜tool▁call▁end｜>";
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
    //"{% endif %}";
    }
    return result.view();
}
