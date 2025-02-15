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
    static BackendState& instance()
    {
        static BackendState state;
        return state;
    }
    static void tls_info_callback(const SSL* ssl, int where, int ret)
    {
        if (where & SSL_CB_ALERT) {
            if ((ret>>8) == SSL3_AL_WARNING && (ret&0xff) == SSL_AD_CLOSE_NOTIFY) {
                std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
                auto socket = (net::ip::tcp::socket*)SSL_CTX_get_ex_data(SSL_get_SSL_CTX(ssl), 1);
                socket->close();
            }
        }
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
        if (path.empty()) {
            path = "/";
        }
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
//*/

template<typename StreamType>
struct LoanedConnection
{
    // Shared helper functions for common setup steps
    LoanedConnection(URL const& url)
    : url(url)
    , check_res_parser(false)
    {
        connect();
    }
    void request(const std::string_view method, std::string_view body, std::span<HTTP::Header const> headers)
    {
        req = http::request<http::string_body>{method == "GET" ? http::verb::get : http::verb::post, url.path, 11};
        req.set(http::field::host, url.host);
        req.set(http::field::user_agent, "zinc-http-client");
        req.set(http::field::connection, "keep-alive");
        req.set(http::field::keep_alive, "timeout=3600");
        for (const auto& [key, value] : headers) {
            req.set(key.data(), value.data());
        }
        if (!body.empty() && req.method() == http::verb::post) {
            req.body() = body;
        }
        req.prepare_payload();
        //if (body.empty() || method == "GET") {
        http::write(*stream, req);
        //} else {
        //    http::serializer<false, http::string_body> sr{req};
        //    http::write_header(*stream, sr);
        //    http::write(*stream, sr);
        //}
    }
    std::string http_string(std::string_view method, std::string_view req_body, std::span<HTTP::Header const> headers)
    {
        http::response<http::dynamic_body> res;
        request(method, req_body, headers);
        try {
            http::read(*stream, buffer, res);
        } catch (boost::system::system_error & se) {
            if (res.body().size() == 0 && buffer.size() == 0) {
                switch (se.code().value()) {
                default:
                    throw;
                case net::error::no_permission:
                case net::error::eof:
                case net::error::connection_reset:
                    connect();
                    request(method, req_body, headers);
                    http::read(*stream, buffer, res);
                }
            } else {
                throw;
            }
        }

        std::string res_body = buffers_to_string(res.body().data());
        if (res.result_int() / 100 != 2) {
            throw std::runtime_error(std::string(res.reason()) + res_body);
        }
        return res_body;
    }
    zinc::generator<std::string_view> http_lines(std::string_view method, std::string_view req_body, std::span<HTTP::Header const> headers)
    {
        auto& res = res_parser.get();
        auto& res_buffer = res.body();
        request(method, req_body, headers);
        try {
            http::read_header(*stream, buffer, res_parser);
        } catch (boost::system::system_error & se) {
            if (res_buffer.size() == 0 && buffer.size() == 0) {
                switch (se.code().value()) {
                default:
                    throw;
                case net::error::no_permission:
                case net::error::eof:
                case net::error::connection_reset:
                    connect();
                    request(method, req_body, headers);
                    http::read_header(*stream, buffer, res_parser);
                }
            } else {
                throw;
            }
        }

        if (res.result_int() / 100 != 2) {
            http::read(*stream, buffer, res_parser);
            throw std::runtime_error(std::string(res.reason()) + beast::buffers_to_string(res.body().data()));
        }

        check_res_parser = true;

        while (!res_parser.is_done()) {
            size_t bytesRead = http::read_some(*stream, buffer, res_parser);
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

                co_yield std::string_view(data.data() + start, end - start);

                start = end + 1;
            }

            if (start > 0) {
                res_buffer.consume(start);
            }
        }

        if (res_buffer.size() > 0) {
            co_yield std::string_view((char const*)res_buffer.cdata().data(), res_buffer.size());
        }

        co_return;
    }
    ~LoanedConnection()
    {
        if (check_res_parser) {
            if (!res_parser.get().keep_alive()) {
                return;
            }
            if (!res_parser.is_done() && buffer.size()) {
                // we try draining the buffer in case we are near the end
                // but if multiple iterations are needed it could cause a hang and should be done in a background thread.
                http::read_some(*stream, buffer, res_parser);
            }
            if (!res_parser.is_done()) {
                return;
            }
        }
        if (connected()) {
            std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
            auto it = BackendState::instance().connection_cache.find(key);
            if (it != BackendState::instance().connection_cache.end()) {
                // because we know this one is connected, erasing the other is reasonable, but it could be useful to keep them both
                BackendState::instance().connection_cache.erase(it);
            }
            it = BackendState::instance().connection_cache.emplace(key,std::move(static_cast<StreamType&>(*this->stream))).first;
            if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
                StreamType & streamref = std::get<StreamType>(it->second);
                SSL_CTX_set_ex_data(SSL_get_SSL_CTX(streamref.native_handle()), 1, &socket(streamref));
            }
        }
    }

    URL url;
    std::string key;
    std::optional<StreamType> stream;
    http::request<http::string_body> req;
    beast::flat_buffer buffer;
    bool check_res_parser;
    http::response_parser<http::basic_dynamic_body<beast::flat_buffer>> res_parser;
private:
    void connect()
    {
        net::io_context& ioc = BackendState::instance().ioc;
        {
            std::stringstream ss;
            ss << (url.tls ? "https://" : "http://") << url.host << ":" << url.port;
            key = ss.str();
        }
        // Check if connection is cached
        {
            std::lock_guard<std::mutex> lock(BackendState::instance().mtx);
            auto it = BackendState::instance().connection_cache.find(key);
            if (it != BackendState::instance().connection_cache.end()) {
                if (std::holds_alternative<StreamType>(it->second)) {
                    stream.emplace(std::move(std::get<StreamType>(it->second)));
                    if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) { 
                        SSL_CTX_set_ex_data(SSL_get_SSL_CTX(stream->native_handle()), 1, &socket(*stream));
                    }
                    BackendState::instance().connection_cache.erase(it);
                    if (connected()) { return; }
                }
            }
        }
        auto const results = tcp::resolver(ioc).resolve(url.host, url.port);
        if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) { 
            ssl::context ctx{ssl::context::tlsv12_client};
            ctx.set_default_verify_paths();
            stream.emplace(ioc, ctx);
            if (!SSL_set_tlsext_host_name(stream->native_handle(), url.host.c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }
            SSL_CTX_set_ex_data(ctx.native_handle(), 1, &socket(*stream));
            SSL_CTX_set_info_callback(ctx.native_handle(), BackendState::tls_info_callback);
            //*/SSL_CTX_set_keylog_callback(ctx.native_handle(), ssl_key_log_callback);//*/
            net::connect(socket(*stream), results.begin(), results.end());
            socket(*stream).set_option(net::socket_base::keep_alive(true));
            stream->handshake(ssl::stream_base::client);
        } else { 
            stream.emplace(ioc);
            net::connect(socket(*stream), results.begin(), results.end());
            socket(*stream).set_option(net::socket_base::keep_alive(true));
        }
    }

    bool connected() {
        auto & socket = this->socket(*stream);
        if (!socket.is_open()) {
            return false;
        }
        int sockfd = socket.native_handle();
        if (sockfd == -1) {
            return false;
        }
        int error = 0;
        socklen_t len = sizeof(error);
        int r = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
        if (r != 0 || error != 0) {
            return false;
        }
        struct tcp_info tcp_info;
        len = sizeof(tcp_info);
        r = getsockopt(sockfd, SOL_TCP, TCP_INFO, &tcp_info, &len);
        if (r == 0 && tcp_info.tcpi_state == TCP_CLOSE_WAIT) {
            return false;
        }
        return true;
    }

    static auto & socket(StreamType& stream) {
        if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
            return stream.next_layer().socket();
        } else if constexpr (std::is_same_v<StreamType, beast::tcp_stream>) {
            return stream.socket();
        }
    }
};

std::string HTTP::request_string(std::string_view method, std::string_view url_str, std::string_view body, std::span<Header const> headers) {
    URL url{url_str};
    std::string response;
    std::string key;
    if (url.tls) {
        LoanedConnection<beast::ssl_stream<beast::tcp_stream>> loan(url);
        return loan.http_string(method, body, headers);
    } else {
        LoanedConnection<beast::tcp_stream> loan(url);
        return loan.http_string(method, body, headers);
    }
}

zinc::generator<std::string_view> HTTP::request_lines(std::string_view method, std::string_view url_str, std::string_view body, std::span<Header const> headers) {
    URL url{url_str};
    std::string key;
    http::request<http::string_body> req;

    if (url.tls) {
        LoanedConnection<beast::ssl_stream<beast::tcp_stream>> loan(url);
        co_yield zinc::ranges::elements_of(loan.http_lines(method, body, headers));
    } else {
        LoanedConnection<beast::tcp_stream> loan(url);
        co_yield zinc::ranges::elements_of(loan.http_lines(method, body, headers));
    }
    co_return;
}
} // namespace zinc
