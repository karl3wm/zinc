#include <zinc/hodgepodge.hpp>
#include <zinc/http.hpp>
#include <zinc/openai.hpp>
#include <zinc/log.hpp>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;
using namespace zinc;

std::string perform_replacements(std::string_view message) {
    std::stringstream new_msg;
    std::stringstream new_docs;
    static size_t nrefs = 0;
    size_t last_off = 0;
    for (auto && [off, match] : zinc::find_all_of(message, zinc::span<string_view>({"$(", /*"`", */"http"}))) {
        //decltype(message)::iterator tail;
        size_t tailpos;
        switch(message[off]) {
        case '$': // $(
            if (message.size() - off <= 4) continue;
            tailpos = message.find(')', off);
            if (tailpos == std::string_view::npos) continue;
            break;
        case '`': // `
            if (message.size() - off <= 2) continue;
            tailpos = message.find('`', off);
            if (tailpos == std::string_view::npos) continue;
            break;
        case 'h': // http
            if (message.size() - off <= 8) {
                continue;
            }
            if (message.substr(off,7) == "http://" || message.substr(off,8) == "https://") {
                auto [suboff, _] = zinc::find_first_of(message.substr(off), zinc::span<std::string_view>({" ","\t","\n",",",";","!","'","\""}));
                if (suboff == std::string_view::npos) {
                    tailpos = message.size();
                } else {
                    tailpos = off + suboff;
                    switch(message[tailpos-1]) {
                    case '.':
                    case '?':
                        -- tailpos;
                    }
                    break;
                }
            } else {
                continue;
            }
            break;
        }
        //tail = (tailpos == std::string_view::npos) ? message.end() : (message.begin() + tailpos);
        new_msg << message.substr(last_off, off - last_off);
        last_off = off;
        ++ nrefs;
        new_msg << "[" << nrefs << "]";
        new_docs << "\n";
        switch(message[off]) {
        case '$': // $(
        {
            off += 2;
            if (message[off] == '<') {
                // file
                off += 1;
                std::string_view fn = message.substr(off, tailpos - off);
                std::cerr << nrefs << ": " << fn << std::endl;
                ++ tailpos;
                new_docs << nrefs << ": " << fn << "\n\n**" << fn << "**\n";
                new_docs << "```\n";
                std::ifstream f{std::filesystem::path(fn)};
                new_docs << f.rdbuf();
                new_docs << "\n```\n";
            } else {
                // commands
                std::string_view cmds = message.substr(off, tailpos - off);
                std::cerr << nrefs << ": $ " << cmds << std::endl;
                ++ tailpos;
                new_docs << nrefs << ":\n";
                new_docs << "```\n";
                new_docs << "$ " << cmds << "\n";
                for (auto && chunk : shell(cmds)) {
                    new_docs << chunk;
                }
                new_docs << "\n```\n";
            }
            break;
        }
        case '`': // `
        {
            off += 1;
            std::string_view cmds = message.substr(off, tailpos - off);
            ++ tailpos;
            new_docs << nrefs << ":\n";
            new_docs << "```\n";
            new_docs << "$ " << cmds << "\n";
            for (auto && chunk : shell(cmds)) {
                new_docs << chunk;
            }
            new_docs << "\n```\n";
            break;
        }
        case 'h': // http
        {
            std::string_view url = message.substr(off, tailpos - off);
            std::cerr << nrefs << ": " << url << std::endl;
            new_docs << nrefs << ": **" << url << "**\n";
            new_docs << "```\n";
            try {
                for (auto && line : zinc::HTTP::request_lines("GET", url)) {
                    new_docs << line << "\n";
                }
            } catch (std::runtime_error const& er) {
                new_docs << er.what();
                std::cerr << er.what() << std::endl;
            }
            new_docs << "```\n";
            break;
        }
        }
        if (tailpos == std::string_view::npos) {
            last_off = message.size();
        } else {
            last_off = tailpos;
        }
    }
    new_msg << message.substr(last_off);
    return new_msg.str() + "\n" + new_docs.str();
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char **argv) {
    std::cerr << "The following will be replaced:" << std::endl;
    std::cerr << "- $(<pathname) will read a file" << std::endl;
    std::cerr << "- $(commands) will capture output from commands" << std::endl;
    std::cerr << "- `commands` is _not_ interpreted due to other use in llms" << std::endl;
    std::cerr << "- http://url will fetch a url" << std::endl;
    std::cerr << "- https://url will also fetch a url" << std::endl;
    std::cerr << "Presently no escaping is processed, so be careful not to end commands early." << std::endl;
    std::cerr << std::endl;

    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    OpenAI client(
            //"https://api.sambanova.ai",
            ////"Meta-Llama-3.1-405B-Instruct",
            //"Llama-3.1-Tulu-3-405B",
            //"b8736160-f724-4b68-bac0-aea866ceee15",
            "https://api.targon.com", // url
            //"deepseek-ai/DeepSeek-R1", // model
            "deepseek-ai/DeepSeek-V3", // model
            "sn4_qij7ph9d6yy9ttdizx66x0n5ld96", // key
            //"https://chutes-deepseek-ai-deepseek-r1.chutes.ai", //url
            //"deepseek-ai/DeepSeek-R1", //model
            //"cpk_f6d4acdfdd9e4dca97cc1068ee548e5d.741ff3f12d3e5331b8ac0e1140401452.isBWVv07rRtrxTZVvXi0Wlt7asipainR", //key
            zinc::span<KeyJSONPair>({{"max_completion_tokens", 512}})
    );

    //vector<OpenAI::RoleContentPair> messages;
    vector<HodgePodge::Message> messages;
    string msg, input;
    int retry_assistant;

    for (int i = 1; i < argc; ++ i) {
        msg = msg + "$(<" + argv[i] + ") ";
    }

    std::string prompt;
    while ("end of input not reached") {
        cerr << endl << "user: " << flush;
        if (!msg.empty()) {
            cerr << msg << flush;
        }
        {
            getline(cin, input);
            msg += input;
            streamsize extra;
            if (!cin) {
                break;
            } else while ((extra = cin.rdbuf()->in_avail()) > 1) {
                size_t msg_size = msg.size();
                msg.resize(msg_size + (size_t)extra);
                cin.read(&msg[msg_size], extra);
            }
            if (msg.back() == '\n') {
                msg.resize(msg.size() - 1);
            }
            msg = perform_replacements(msg);
        }

        Log::log(zinc::span<StringViewPair>({
            {"role", "user"},
            {"content", msg},
        }));

        messages.emplace_back(HodgePodge::Message{.role="user", .content=move(msg)});
        // it might be nice to terminate the request if more data is found on stdin, append the data, and retry
        // or otherwise provide for the user pasting some data then commenting on it or hitting enter a second time or whatnot
        prompt = HodgePodge::prompt_deepseek3(messages, "assistant" != messages.back().role);
        msg.clear();

        cerr << endl << "assistant: " << flush;

        // Install signal handler for SIGINT
        static sig_atomic_t SIGINT_RAISED;
        SIGINT_RAISED = false;
        std::signal(SIGINT, [](int){
            std::signal(SIGINT, SIG_DFL);
            SIGINT_RAISED = true;
        });

        do {
            ssize_t chunk_start = (ssize_t)msg.size();
            std::string finish_reason;
            std::string finish_data;
            retry_assistant = true;
            try {
                //for (auto&& part : client.chat(messages)) {
                for (auto&& part : client.complete(prompt + msg)) {
                    msg += part;
                    cout << part << flush;
                    auto fr = part.data.dicty("finish_reason");
                    finish_data = part.data.encode();
                    if (fr.truthy()) {
                        finish_reason = fr.string();
                        if (finish_reason == "stop") {
                            retry_assistant = false;
                        } else {
                            cerr << "<...finish_reason=" << finish_reason << "...>" << flush;
                        }
                    }
                    if (SIGINT_RAISED) {
                        throw std::runtime_error("SIGINT");
                    }
                    if (finish_reason == "" && (ssize_t)msg.size() == chunk_start) {
                        retry_assistant = false;
                        throw std::runtime_error(finish_data);
                    }
                }
            } catch (std::runtime_error const& e) {
                cerr << "<..." << e.what();
                finish_reason = std::string("runtime_error ") + e.what();
                if (!SIGINT_RAISED) {
                    cerr << "...reconnecting";
                }
                cerr << "...>" << flush;
            /*} catch (std::system_error const& e) {
                if (e.code() == std::errc::resource_unavailable_try_again) {
                    retry_assistant = true;
                    cerr << "<...reconnecting...>" << flush;
                } else {
                    throw;
                }*/
            }

            Log::log(zinc::span<StringViewPair>({
                {"role", "assistant"},
                {"content", std::string_view(msg.begin() + chunk_start, msg.end())},
                {"finish_reason", finish_reason.empty() ? finish_data : finish_reason},
            }));
        } while (retry_assistant && !SIGINT_RAISED);
        // Restore the default signal handler
        std::signal(SIGINT, SIG_DFL);

        messages.emplace_back(HodgePodge::Message{.role="assistant", .content=move(msg)});
        msg.clear();

        cout << endl;
    }

    return 0;
}
