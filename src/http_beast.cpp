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

struct URL {
    std::string host, path;
    std::string_view port;
    bool tls;
};

// Helper function to parse URL and extract components
URL parse_url(std::string_view url_str) {
    auto url_result = url::parse_uri(url_str);
    if (!url_result) {
        throw std::runtime_error("Invalid URL format");
    }
    url::url_view url = *url_result;
    bool tls = (url.scheme_id() == url::scheme::https);
    std::string_view port = url.port();
    if (port.empty()) {
        port = tls ? "443" : "80";
    }
    return {
        url.host(), url.path(),
        port,
        tls
    };
}


// Helper function to add custom headers to the request
void add_headers(http::request<http::string_body>& req, std::span<const std::pair<std::string_view, std::string_view>> headers) {
    for (const auto& [key, value] : headers) {
        req.set(key.data(), value.data());
    }
}

// Shared helper functions for common setup steps
template<typename StreamType>
StreamType connect(URL const& url, std::string& key) {
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
            if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
                if (std::holds_alternative<beast::ssl_stream<beast::tcp_stream>>(it->second)) {
                    StreamType stream = std::move(std::get<beast::ssl_stream<beast::tcp_stream>>(it->second));
                    BackendState::instance().connection_cache.erase(it);
                    if (stream.next_layer().socket().is_open()) { return stream; }
                }
            } else if constexpr (std::is_same_v<StreamType, beast::tcp_stream>) {
                if (std::holds_alternative<beast::tcp_stream>(it->second)) {
                    StreamType stream = std::move(std::get<beast::tcp_stream>(it->second));
                    BackendState::instance().connection_cache.erase(it);
                    if (stream.socket().is_open()) { return stream; }
                }
            }
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
        net::connect(stream.next_layer().socket(), results.begin(), results.end());
        stream.handshake(ssl::stream_base::client);
        return stream;
    } else { 
        StreamType stream{ioc}; 
        net::connect(stream.socket(), results.begin(), results.end());
        return stream;
    }
}
template<typename StreamType>
StreamType prepare_request(const std::string_view method, URL const& url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body,
                     http::request<http::string_body>& req, std::string& key) {

    StreamType stream = connect<StreamType>(url, key);

    req = http::request<http::string_body>{method == "GET" ? http::verb::get : http::verb::post, url.path, 11};
    req.set(http::field::host, url.host);
    req.set(http::field::user_agent, "zinc-http-client");
    add_headers(req, headers);
    req.set(http::field::connection, "keep-alive");
    if (!body.empty() && method == "POST") {
        req.body() = std::string(body);
        req.prepare_payload();
    }

    return stream;
}

template<typename StreamType>
std::string execute_request(StreamType& stream, const http::request<http::string_body>& req) {
    beast::flat_buffer buffer;
    http::write(stream, req);
    http::response<http::dynamic_body> res;
    http::read(stream, buffer, res);
    return boost::beast::buffers_to_string(res.body().data());
}

template<typename StreamType>
void handle_response(const std::string& key, StreamType& stream) {
    std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
    if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
        BackendState::instance().connection_cache.emplace(key,std::move(static_cast<beast::ssl_stream<beast::tcp_stream>&>(stream)));
    } else {
        BackendState::instance().connection_cache.emplace(key,std::move(static_cast<beast::tcp_stream&>(stream)));
    }
}

std::string HttpClient::request_string(std::string_view method, std::string_view url_str, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body) {
    URL url = parse_url(url_str);
    std::string response;
    std::string key;
    if (url.tls) {
        ssl::context ctx{ssl::context::tlsv12_client};
        http::request<http::string_body> req;
        auto stream = prepare_request<beast::ssl_stream<beast::tcp_stream>>(method, url, headers, body, req, key);
        response = execute_request(stream, req);
        handle_response(key, stream);
    } else {
        http::request<http::string_body> req;
        auto stream = prepare_request<beast::tcp_stream>(method, url, headers, body, req, key);
        response = execute_request(stream, req);
        handle_response(key, stream);
    }
    return response;
}

template <typename StreamType>
std::generator<std::string_view> process_response(StreamType& stream, beast::flat_buffer& rotate_buffer, http::response_parser<http::basic_dynamic_body<beast::flat_buffer>>& res_parser) {
    auto& res_buffer = res_parser.get().body();
    while (!res_parser.is_done()) {
        /*size_t bytesRead = */http::read_some(stream, rotate_buffer, res_parser);

        if (res_buffer.size() == 0) {
            break;
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

    co_return;
}

std::generator<std::string_view> HttpClient::request(std::string_view method, std::string_view url_str, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body) {
    URL url = parse_url(url_str);
    std::string key;
    http::request<http::string_body> req;
    beast::flat_buffer rotate_buffer;
    http::response_parser<http::basic_dynamic_body<beast::flat_buffer>> res_parser;

    if (url.tls) {
        ssl::context ctx{ssl::context::tlsv12_client};
        auto stream = prepare_request<beast::ssl_stream<beast::tcp_stream>>(method, url, headers, body, req, key);
        http::write(stream, req);
        http::read_header(stream, rotate_buffer, res_parser);
        for (auto line : process_response(stream, rotate_buffer, res_parser)) {
            co_yield line;
        }
        handle_response(key, stream);
    } else {
        auto stream = prepare_request<beast::tcp_stream>(method, url, headers, body, req, key);
        http::write(stream, req);
        http::read_header(stream, rotate_buffer, res_parser);
        for (auto line : process_response(stream, rotate_buffer, res_parser)) {
            co_yield line;
        }
        handle_response(key, stream);
    }
    co_return;
}
} // namespace zinc
