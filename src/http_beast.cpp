#include <zinc/http.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/url.hpp>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <fstream>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace url = boost::urls;

namespace zinc {

// Static object to hold io_context, mutex for thread safety, and connection cache
struct BackendState {
    net::io_context ioc;
    std::mutex mtx;
    std::unordered_map<std::string, std::variant<beast::tcp_stream, beast::ssl_stream<beast::tcp_stream>>> connection_cache;
    static BackendState& instance() {
        static BackendState state;
        return state;
    }
};

// Helper class to parse URL and extract components
struct URL {
    URL(std::string_view url_str)
    {
        auto url_result = url::parse_uri(url_str);
        if (!url_result) {
            throw std::runtime_error("Invalid URL format");
        }
        url::url_view url = *url_result;
        tls = (url.scheme_id() == url::scheme::https);
        port = url.port();
        if (port.empty()) {
            port = tls ? "443" : "80";
        }
        host = url.host();
        path = url.path();
    }
    std::string host, path;
    std::string_view port;
    bool tls;
};

/*
// this can cache ssl keys for packet debugging, enabled by searching for function name in file and uncommenting
void ssl_key_log_callback(const SSL *, const char *line) {
    static std::ofstream keyLogFile("sslkeylog.log", std::ios_base::app);
    if (keyLogFile.is_open()) {
        keyLogFile << line << std::endl;
    } else {
        std::cerr << "Failed to open SSL key log file." << std::endl;
    }
}
*/

template<typename StreamType>
struct LoanedConnection
{
    // Shared helper functions for common setup steps
    LoanedConnection(URL const& url)
    : stream(connect(url))
    { }
    LoanedConnection(LoanedConnection<StreamType>&&)=default;
    void request(const std::string_view method, URL const& url, std::string_view body, std::span<const std::pair<std::string_view, std::string_view>> headers)
    {
        req = http::request<http::string_body>{method == "GET" ? http::verb::get : http::verb::post, url.path, 11};
        req.set(http::field::host, url.host);
        req.set(http::field::user_agent, "zinc-http-client");
        for (const auto& [key, value] : headers) {
            req.set(key.data(), value.data());
        }
        req.set(http::field::connection, "keep-alive");
        if (!body.empty() && method == "POST") {
            req.body() = std::string(body);
            req.prepare_payload();
        }
        http::write(stream, req);
    }
    std::string http_string()
    {
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);
        std::string body = buffers_to_string(res.body().data());
        if (res.result_int() / 100 != 2) {
            throw std::runtime_error(std::string(res.reason()) + body);
        }
        return body;
    }
    zinc::generator<std::string_view> http_lines()
    {
        /*
         so, the general problem is that the wrapper can have local
         variables, but these go out of scope before they are passed
         to the coroutine.
         my solution was kind of better.
         this solution can work so far, maybe with a pointer to the heap.
         the first question for making it work is where the state is
         stored, between the wrapper function and the coroutine.
         the coroutine is not executed until the local data is out of scope,
         but the generator must be constructed to be returned, so it could
         be put on the generator. this would be a custom generator type.
         the two objects that persist are the generator and its promise.
         these objects are likely on the heap.
         so basically we're using the heap for normal function calls.
         but the compiler can optimize the generator into the stack.
         so we don't have to, if we stick with the contents of the generator
         and the promise.
         
         but without access to the insides of the generator, we can't
         change its size to put more things on the stack.

         a solution is to yield a sentinal value.
         this ensures the coroutine is resumed.
         it's comparable to manually resuming which would drop the first
         value.

         we'd then have to return a subrange that excludes the sentinal value.

         alternativelly, we could store the loan statically, and move it out.
            
         right, we can use static storage as an interim value.
         we can use a static vector, and store the offset.
         */

        // transfer loan to the coroutine stack
        // keep loan on stack after caller returns the generator
        
        // the solution is not elegant at all.
        // handling the exception correctly seems like a better solution.

        // for every call we need to maintain static storage unique to the call
        // and use the static storage to hand values off to the implementation

        // maybe i'll see what an implementation looks like

        static thread_local std::vector<LoanedConnection<StreamType>>(16);
            // the http_lines_ function needs to move its loan out of the
            // storage. after it is called.
            // lets move it out of the class?
            // then we can pass the vector to it more appropriately ...
            // maybe it doesn't matter ...
                // a concern is that if its stored in a vector its
                // 'this' pointer can change when the vector is resized.
                // so we might want to disallow interleaving of generators :s
                // or we could allocate an array
                // or we could update the this pointers when moving
                // that woudl actually work, reseating the this pointers :s
            // the elegant solution sounds better
            // time for two branches
        LoanedConnection<StreamType> loan = std::move(*this);
        return loan.http_lines_();
    }
    ~LoanedConnection()
    {
        if (socket(stream).is_open()) {
            std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
            BackendState::instance().connection_cache.emplace(key, std::move(static_cast<StreamType&>(stream)));
        }
    }

    static boost::beast::tcp_stream::socket_type & socket(StreamType & stream)
    {
        if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
            return stream.next_layer().socket();
        } else {
            return stream.socket();
        }
    }

    std::string key;
    StreamType stream;
    http::request<http::string_body> req;
    beast::flat_buffer buffer;

private:

    StreamType connect(URL const& url)
    {
        net::io_context& ioc = BackendState::instance().ioc;
        {
            std::stringstream ss;
            ss << (url.tls ? "https://" : "http://") << url.host << ":" << url.port;
            key = std::move(ss.str());
        }
        // Check if connection is cached
        {
            std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
            auto it = BackendState::instance().connection_cache.find(key);
            if (it != BackendState::instance().connection_cache.end()) {
                StreamType stream = std::move(std::get<StreamType>(it->second));
                BackendState::instance().connection_cache.erase(it);
                if (socket(stream).is_open()) { return stream; }
            }
        }
        auto const results = net::ip::tcp::resolver(ioc).resolve(url.host, url.port);
        if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) { 
            ssl::context ctx{ssl::context::tlsv12_client};
            ctx.set_default_verify_paths();
            StreamType stream{ioc, ctx};
            if (!SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }
            //SSL_CTX_set_keylog_callback(ctx.native_handle(), ssl_key_log_callback);
            net::connect(stream.next_layer().socket(), results.begin(), results.end());
            stream.handshake(ssl::stream_base::client);
            return stream;
        } else { 
            StreamType stream{ioc}; 
            net::connect(stream.socket(), results.begin(), results.end());
            return stream;
        }
    }
    zinc::generator<std::string_view> http_lines_()
    {
        struct parser_raii_flushed
        : public http::response_parser<http::basic_dynamic_body<beast::flat_buffer>>
        {
            parser_raii_flushed(LoanedConnection<StreamType>&loan)
            : loan(loan)
            {
                http::read_header(loan.stream, loan.buffer, *this);
                if (get().result_int() / 100 != 2) {
                    http::read(loan.stream, loan.buffer, *this);
                    throw std::runtime_error(std::string(get().reason()) + beast::buffers_to_string(get().body().data()));
                }
            }

            ~parser_raii_flushed()
            {
                while (loan.buffer.size()) {
                    http::read_some(loan.stream, loan.buffer, *this);
                }
                if (!is_done()) {
                    socket(loan.stream).close();
                }
            }
            
            LoanedConnection<StreamType> & loan;
        } res_parser(*this);

        auto& res_buffer = res_parser.get().body();

        while (!res_parser.is_done()) {
            size_t bytesRead = http::read_some(stream, buffer, res_parser);
            if (res_buffer.size() == 0) {
                if (bytesRead == 0) {
                    break;
                } else {
                    continue;
                }
            }

            std::string_view data((char const*)res_buffer.cdata().data(), res_buffer.size());
            size_t start = 0;
            while ("lines in chunk") {
                size_t end = data.find('\n', start);
                if (end == std::string_view::npos) {
                    break;
                }

                co_yield {data.data() + start, end - start};


                start = end + 1;
            }

            if (start > 0) {
                res_buffer.consume(start);
            }
        }

        if (res_buffer.size() > 0) {
            co_yield {(char const*)res_buffer.cdata().data(), res_buffer.size()};
        }
    }
};

std::string HTTP::request_string(std::string_view method, std::string_view url_str, std::string_view body, std::span<const std::pair<std::string_view, std::string_view>> headers) {
    URL url{url_str};
    std::string response;
    std::string key;
    if (url.tls) {
        LoanedConnection<beast::ssl_stream<beast::tcp_stream>> loan(url);
        loan.request(method, url, body, headers);
        return loan.http_string();
    } else {
        LoanedConnection<beast::tcp_stream> loan(url);
        loan.request(method, url, body, headers);
        return loan.http_string();
    }
}

zinc::generator<std::string_view> HTTP::request_lines(std::string_view method, std::string_view url_str, std::string_view body, std::span<const std::pair<std::string_view, std::string_view>> headers) {
    URL url{url_str};
    std::string key;
    http::request<http::string_body> req;

    if (url.tls) {
        LoanedConnection<beast::ssl_stream<beast::tcp_stream>> loan(url);
        loan.request(method, url, body, headers);
        return loan.http_lines();
    } else {
        LoanedConnection<beast::tcp_stream> loan(url);
        loan.request(method, url, body, headers);
        return loan.http_lines();
    }
}
} // namespace zinc

// vim: set ts=4 sw=4 et:
