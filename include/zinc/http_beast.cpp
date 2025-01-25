// http_beast.cpp
#include "http.hpp"
#include <asio.hpp>
#include <boost/beast.hpp>
#include <boost/url.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <span>
#include <utility>
#include <mutex>
#include <unordered_map>

namespace net = asio;
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
namespace url = boost::urls;

namespace zinc {

// Helper function to parse URL and extract components
std::tuple<std::string, std::string, std::string> parse_url(std::string_view url_str) {
    auto url_result = url::parse_uri(url_str);
    if (!url_result) {
        throw std::runtime_error("Invalid URL format");
    }
    const url::url_view url = *url_result;

    std::string scheme = url.scheme();
    std::string host = url.host();
    std::string path = url.path();
    if (path.empty()) {
        path = "/";
    }

    return {scheme, host, path};
}

// Static object to hold io_context, mutex for thread safety, and connection cache
struct BackendState {
    net::io_context ioc;
    std::mutex mtx;
    std::unordered_map<std::string, beast::tcp_stream> connection_cache;

    static BackendState& instance() {
        static BackendState state;
        return state;
    }
};

// Helper function to set up the connection and perform SSL handshake if necessary
beast::tcp_stream setup_connection(net::io_context& ioc, const std::string& host, const std::string& port, bool use_ssl) {
    auto const results = net::ip::tcp::resolver(ioc).resolve(host, port);
    beast::tcp_stream stream{ioc};
    stream.connect(results);

    if (use_ssl) {
        ssl::context ctx{ssl::context::tlsv12_client};
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{ec};
        }

        ssl::stream<beast::tcp_stream> ssl_stream{std::move(stream), ctx};
        ssl_stream.handshake(ssl::stream_base::client);
        stream = std::move(ssl_stream.next_layer());
    }

    return stream;
}

// Helper function to add custom headers to the request
void add_headers(http::request<http::string_body>& req, std::span<const std::pair<std::string_view, std::string_view>> headers) {
    for (const auto& [key, value] : headers) {
        req.set(key.data(), value.data());
    }
}

// Shared helper function for common setup steps
void prepare_request(const std::string& method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body,
                     beast::tcp_stream& stream, http::request<http::string_body>& req, std::string& key) {
    auto [scheme, host, path] = parse_url(url);
    net::io_context& ioc = BackendState::instance().ioc;
    key = scheme + "://" + host;

    // Setup connection outside of the mutex lock to avoid potential deadlocks during DNS resolution
    bool new_connection = true;
    stream = setup_connection(ioc, host, scheme == "https" ? "443" : "80", scheme == "https");

    // Inside the mutex lock only for accessing the connection cache
    {
        std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
        auto it = BackendState::instance().connection_cache.find(key);
        if (it != BackendState::instance().connection_cache.end()) {
            stream = std::move(it->second);
            BackendState::instance().connection_cache.erase(it);
            new_connection = false;
        }
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

std::string HttpClient::request_string(std::string_view method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body) {
    std::string response;
    beast::tcp_stream stream;
    http::request<http::string_body> req;
    std::string key;

    try {
        prepare_request(std::string(method), url, headers, body, stream, req, key);

        beast::flat_buffer buffer;
        http::write(stream, req);

        http::response<http::dynamic_body> res;
        http::read_header(stream, buffer, res);

        response.assign(boost::beast::buffers_to_string(res.body().data()));

        // Check if the connection should be kept alive
        if (req[http::field::connection] != "close" && res[http::field::connection] != "close") {
            std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
            BackendState::instance().connection_cache[key] = std::move(stream);
        } else {
            beast::error_code ec;
            stream.shutdown(ec);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return response;
}

std::generator<std::string_view> HttpClient::request(std::string_view method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers, std::string_view body) {
    beast::tcp_stream stream;
    http::request<http::string_body> req;
    std::string key;

    try {
        prepare_request(std::string(method), url, headers, body, stream, req, key);

        beast::flat_buffer buffer;
        http::write(stream, req);

        http::response<http::dynamic_body> res;
        http::read_header(stream, buffer, res);

        std::string remaining_data;
        while (true) {
            beast::flat_buffer chunk_buffer;
            http::read_some(stream, chunk_buffer, res.body().data());

            std::string_view body_data(reinterpret_cast<const char*>(res.body().data().data()),
                                       res.body().data().size());

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

        // Check if the connection should be kept alive
        if (req[http::field::connection] != "close" && res[http::field::connection] != "close") {
            std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
            BackendState::instance().connection_cache[key] = std::move(stream);
        } else {
            beast::error_code ec;
            stream.shutdown(ec);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

} // namespace zinc
