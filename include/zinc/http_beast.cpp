#include "http.hpp"
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

// Helper function to parse URL and extract components
std::tuple<std::string, std::string, std::string, std::string> parse_url(std::string_view url_str) {
    auto url_result = url::parse_uri(url_str);
    if (!url_result) {
        throw std::runtime_error("Invalid URL format");
    }
    const url::url_view url = *url_result;
    std::string scheme = url.scheme();
    std::string host = url.host();
    std::string path = url.path();
    std::string port = url.port();
    if (port.empty()) {
        port = (scheme == "https") ? "443" : "80";
    }
    if (path.empty()) {
        path = "/";
    }
    return {scheme, host, path, port};
}

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

// Helper function to set up the connection and perform SSL handshake if necessary
template <typename StreamType>
void setup_connection(StreamType& stream, net::io_context& ioc, const std::string& host, const std::string& port, bool use_ssl) {
    auto const results = net::ip::tcp::resolver(ioc).resolve(host, port);
    net::connect(stream.next_layer(), results.begin(), results.end());
    if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
        ssl::context ctx{ssl::context::tlsv12_client};
        stream.handshake(ssl::stream_base::client);
    }
}

// Helper function to add custom headers to the request
void add_headers(http::request<http::string_body>& req, std::span<const std::pair<std::string_view, std::string_view>> headers) {
    for (const auto& [key, value] : headers) {
        req.set(key.data(), value.data());
    }
}

// Shared helper function for common setup steps
template<typename StreamType>
void prepare_request(const std::string& method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body,
                     StreamType& stream, http::request<http::string_body>& req, std::string& key, bool use_ssl) {
    auto [scheme, host, path, port] = parse_url(url);
    net::io_context& ioc = BackendState::instance().ioc;
    key = scheme + "://" + host + ":" + port;
    // Check if connection is cached
    {
        std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
        auto it = BackendState::instance().connection_cache.find(key);
        if (it != BackendState::instance().connection_cache.end()) {
            if (use_ssl && std::holds_alternative<beast::ssl_stream<beast::tcp_stream>>(it->second)) {
                stream = std::move(std::get<beast::ssl_stream<beast::tcp_stream>>(it->second));
            } else if (!use_ssl && std::holds_alternative<beast::tcp_stream>(it->second)) {
                stream = std::move(std::get<beast::tcp_stream>(it->second));
            } else {
                it = BackendState::instance().connection_cache.end(); // Invalid cache entry
            }
            if (it != BackendState::instance().connection_cache.end()) {
                BackendState::instance().connection_cache.erase(it);
            }
        }
    }
    if (!stream.is_open()) {
        if (use_ssl) {
            stream = StreamType{ioc, ssl::context{ssl::context::tlsv12_client}};
        } else {
            stream = StreamType{ioc};
        }
        setup_connection(stream, ioc, host, port, use_ssl);
    }
    req = http::request<http::string_body>{method == "GET" ? http::verb::get : http::verb::post, path, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "zinc-http-client");
    add_headers(req, headers);
    req.set(http::field::connection, "keep-alive");
    if (!body.empty() && method == "POST") {
        req.body() = std::string(body);
        req.prepare_payload();
    }
}

template<typename StreamType>
std::string execute_request(StreamType& stream, const http::request<http::string_body>& req) {
    beast::flat_buffer buffer;
    beast::write(stream, req);
    http::response<http::dynamic_body> res;
    http::read(stream, buffer, res);
    return boost::beast::buffers_to_string(res.body().data());
}

template<typename StreamType>
void handle_response(const std::string& key, bool use_ssl, StreamType& stream) {
    std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
    if (use_ssl) {
        BackendState::instance().connection_cache[key] = std::move(static_cast<beast::ssl_stream<beast::tcp_stream>&>(stream));
    } else {
        BackendState::instance().connection_cache[key] = std::move(static_cast<beast::tcp_stream&>(stream));
    }
}

std::string HttpClient::request_string(std::string_view method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body) {
    std::string response;
    std::string key;
    bool use_ssl = (url.find("https://") != std::string::npos);
    if (use_ssl) {
        beast::ssl_stream<beast::tcp_stream> stream{BackendState::instance().ioc, ssl::context{ssl::context::tlsv12_client}};
        http::request<http::string_body> req;
        prepare_request(method, url, headers, body, stream, req, key, use_ssl);
        response = execute_request(stream, req);
        handle_response(key, use_ssl, stream);
    } else {
        beast::tcp_stream stream{BackendState::instance().ioc};
        http::request<http::string_body> req;
        prepare_request(method, url, headers, body, stream, req, key, use_ssl);
        response = execute_request(stream, req);
        handle_response(key, use_ssl, stream);
    }
    return response;
}

std::generator<std::string_view> HttpClient::request(std::string_view method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body) {
    std::string key;
    bool use_ssl = (url.find("https://") != std::string::npos);
    std::string remaining_data;
    if (use_ssl) {
        beast::ssl_stream<beast::tcp_stream> stream{BackendState::instance().ioc, ssl::context{ssl::context::tlsv12_client}};
        http::request<http::string_body> req;
        prepare_request(method, url, headers, body, stream, req, key, use_ssl);
        beast::flat_buffer buffer;
        beast::write(stream, req);
        http::response<http::dynamic_body> res;
        http::read_header(stream, buffer, res);
        while (true) {
            beast::flat_buffer chunk_buffer;
            http::read_some(stream, chunk_buffer, res.body().data());
            std::string_view body_data(reinterpret_cast<const char*>(res.body().data().data()), res.body().data().size());
            body_data = remaining_data + body_data;
            size_t start = 0;
            while (true) {
                size_t end = body_data.find('\n', start);
                if (end == std::string_view::npos) {
                    break;
                }
                co_yield std::string_view(body_data.data() + start, end - start);
                start = end + 1;
            }
            remaining_data = std::string(body_data.substr(start));
            if (res.body().data().size() == 0) {
                break;
            }
        }
        handle_response(key, use_ssl, stream);
    } else {
        beast::tcp_stream stream{BackendState::instance().ioc};
        http::request<http::string_body> req;
        prepare_request(method, url, headers, body, stream, req, key, use_ssl);
        beast::flat_buffer buffer;
        beast::write(stream, req);
        http::response<http::dynamic_body> res;
        http::read_header(stream, buffer, res);
        while (true) {
            beast::flat_buffer chunk_buffer;
            http::read_some(stream, chunk_buffer, res.body().data());
            std::string_view body_data(reinterpret_cast<const char*>(res.body().data().data()), res.body().data().size());
            body_data = remaining_data + body_data;
            size_t start = 0;
            while (true) {
                size_t end = body_data.find('\n', start);
                if (end == std::string_view::npos) {
                    break;
                }
                co_yield std::string_view(body_data.data() + start, end - start);
                start = end + 1;
            }
            remaining_data = std::string(body_data.substr(start));
            if (res.body().data().size() == 0) {
                break;
            }
        }
        handle_response(key, use_ssl, stream);
    }
}
} // namespace zinc
