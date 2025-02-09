#include <zinc/http.hpp>
#include <zinc/openai.hpp>
#include <zinc/log.hpp>

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
    for (auto && [off, match] : zinc::find_all_of(message, zinc::span<string_view>({"$(", "`", "http"}))) {
        //decltype(message)::iterator tail;
        size_t tailpos;
        switch(message[off]) {
        case '$': // $(
            tailpos = message.find(')', off);
            if (tailpos == std::string_view::npos) continue;
            break;
        case '`': // `
            tailpos = message.find('`', off);
            if (tailpos == std::string_view::npos) continue;
            break;
        case 'h': // http
            if (message.substr(off,7) == "http://" || message.substr(off,8) == "https://") {
                auto [suboff, _] = zinc::find_first_of(message.substr(off), zinc::span<std::string_view>({" ","\t","\n",",",";","!","'","\""}));
                tailpos = off + suboff;
                switch(message[tailpos-1]) {
                case '.':
                case '?':
                    -- tailpos;
                }
                break;
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
                ++ tailpos;
                new_docs << nrefs << ": **" << fn << "**\n";
                new_docs << "```\n";
                std::ifstream f{std::filesystem::path(fn)};
                new_docs << f.rdbuf();
                new_docs << "\n```\n";
            } else {
                // commands
                std::string_view cmds = message.substr(off, tailpos - off);
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
            new_docs << nrefs << ": **" << url << "**\n";
            new_docs << "```\n";
            for (auto && line : zinc::HTTP::request_lines("GET", url)) {
                new_docs << line << "\n";
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
    std::cerr << "- `commands` is the same as $(commands)" << std::endl;
    std::cerr << "- http://url will fetch a url" << std::endl;
    std::cerr << "- https://url will also fetch a url" << std::endl;
    std::cerr << "Presently no escaping is processed, so be careful not to end commands early." << std::endl;
    std::cerr << std::endl;

    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    string_view url = "https://api.sambanova.ai";
    string_view model = "Meta-Llama-3.1-405B-Instruct";
    string_view key = "d8957211-24e6-426d-90cc-b267ce681e4f";
    OpenAI client(url, model, key);

    vector<OpenAI::RoleContentPair> messages;
    string msg, input;

    for (int i = 1; i < argc; ++ i) {
        msg = msg + "$(<" + argv[i] + ") ";
    }

    while ("end of input not reached") {
        cerr << endl << "user: " << flush;
        if (!msg.empty()) {
            cerr << msg << " " << flush;
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

        messages.emplace_back("user", move(msg));
        // it might be nice to terminate the request if more data is found on stdin, append the data, and retry
        // or otherwise provide for the user pasting some data then commenting on it or hitting enter a second time or whatnot

        cerr << endl << "assistant: " << flush;
        for (auto&& part : client.chat(messages)) {
            msg += part;
            cout << part << flush;
        }
        cout << endl;

        Log::log(zinc::span<StringViewPair>({
            {"role", "assistant"},
            {"content", msg},
        }));

        messages.emplace_back("assistant", move(msg));
    }

    return 0;
}
