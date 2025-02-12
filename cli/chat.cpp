#include <zinc/openai.hpp>
#include <zinc/log.hpp>

#include <iostream>
#include <vector>

using namespace std;
using namespace zinc;

int main([[maybe_unused]]int argc, [[maybe_unused]]char **argv) {
    // Initialize the OpenAI with URL, model, and API key.
    // These values should be replaced
    string_view url = "https://api.sambanova.ai";
    string_view model = "Meta-Llama-3.1-405B-Instruct";
    //"d8957211-24e6-426d-90cc-b267ce681e4f" I got an error around 2025-02-09T07-05:00 that my account had been suspended, deactivated, or deleted. I didn't see any emails. I logged in and didn't see any concerns mentioned in my account interface. I generated a new key.
    ////after the weekend support emailed me and the account is working again. the website looks funny now when i log in, strange numbers and usage history.
    string_view key = "c639ead6-36dd-4f9c-8001-0f1e0c929ab2";

    url = "https://olmo-api.allen.ai";
    model = "tulu3-405b";
    key = "";

    OpenAI client(url, model, key);

    vector<OpenAI::RoleContentPair> messages;
    string msg;

    for (int i = 1; i < argc; ++ i) {
        if (i > 1) msg += " ";
        msg += argv[i];
    }

    while ("end of input not reached") {
        cerr << endl << "user: " << flush;
        if (msg.empty()) {
            getline(cin, msg);
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
        } else {
            cerr << msg << endl;
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
