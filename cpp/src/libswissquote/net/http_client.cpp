#include "swissquote/net/http_client.hpp"

#include <cstdlib>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <variant>

#ifdef SWISSQUOTE_HAVE_OPENSSL
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

namespace swissquote::net {
namespace {

constexpr const char* kHost = "forex-data-feed.swissquote.com";
#ifdef SWISSQUOTE_HAVE_OPENSSL
constexpr int kPort = 443;
#else
constexpr int kPort = 80;
#endif

struct QuoteEndpoint {
    bool tls;
    std::string host;
    int port;
};

QuoteEndpoint quote_endpoint() {
    const char* override_url = std::getenv("SWISSQUOTE_QUOTES_BASE_URL");
    if (override_url == nullptr || *override_url == '\0') {
        return {.tls = true, .host = kHost, .port = kPort};
    }

    const std::string url(override_url);
    const bool tls = url.starts_with("https://");
    const std::string_view scheme = tls ? "https://" : "http://";
    if (!url.starts_with(scheme)) {
        throw std::invalid_argument(
            "SWISSQUOTE_QUOTES_BASE_URL must start with http:// or https://");
    }
    const std::string authority = url.substr(scheme.size());
    if (authority.empty() || authority.find('/') != std::string::npos) {
        throw std::invalid_argument(
            "SWISSQUOTE_QUOTES_BASE_URL must contain only scheme, host, and optional port");
    }

    std::string host = authority;
    int port = tls ? 443 : 80;
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        const std::string port_text = authority.substr(colon + 1);
        if (host.empty() || port_text.empty()) {
            throw std::invalid_argument("invalid SWISSQUOTE_QUOTES_BASE_URL authority");
        }
        try {
            std::size_t consumed = 0;
            port = std::stoi(port_text, &consumed);
            if (consumed != port_text.size() || port < 1 || port > 65535) {
                throw std::invalid_argument("port out of range");
            }
        } catch (const std::exception&) {
            throw std::invalid_argument("invalid SWISSQUOTE_QUOTES_BASE_URL port");
        }
    }
    return {.tls = tls, .host = std::move(host), .port = port};
}

std::string quote_base_url() {
    if (const char* override_url = std::getenv("SWISSQUOTE_QUOTES_BASE_URL");
        override_url != nullptr && *override_url != '\0') {
        return override_url;
    }
    return std::format("https://{}", kHost);
}

} // namespace


std::string quotes_url(std::string_view symbol) {
    return std::format("{}/public-quotes/bboquotes/instrument/{}",
                       quote_base_url(),
                       symbol);
}

#ifdef SWISSQUOTE_HAVE_OPENSSL
using QuoteClient = std::variant<httplib::Client, httplib::SSLClient>;
#endif

struct QuotesClient::Impl {
#ifdef SWISSQUOTE_HAVE_OPENSSL
    QuoteClient client;
#else
    httplib::Client client;
#endif

#ifdef SWISSQUOTE_HAVE_OPENSSL
    explicit Impl()
        : client(std::in_place_type<httplib::Client>, "", 80) {
#else
    explicit Impl() : client(quote_endpoint().host, quote_endpoint().port) {
#endif
        const auto endpoint = quote_endpoint();
#ifdef SWISSQUOTE_HAVE_OPENSSL
        if (endpoint.tls) {
            client.emplace<httplib::SSLClient>(endpoint.host, endpoint.port);
            std::get<httplib::SSLClient>(client).enable_server_certificate_verification(true);
        } else {
            client.emplace<httplib::Client>(endpoint.host, endpoint.port);
        }
        std::visit(
            [](auto& configured_client) {
                configured_client.set_connection_timeout(10, 0);
                configured_client.set_keep_alive(true);
                configured_client.set_read_timeout(30, 0);
            },
            client);
#else
        client.set_connection_timeout(10, 0);
        client.set_keep_alive(true);
        client.set_read_timeout(30, 0);
#endif
    }
};

QuotesClient::QuotesClient() : impl_(std::make_unique<Impl>()) {}

QuotesClient::~QuotesClient() = default;

QuotesClient::QuotesClient(QuotesClient&&) noexcept = default;

QuotesClient& QuotesClient::operator=(QuotesClient&&) noexcept = default;

std::expected<HttpResponse, std::string> QuotesClient::fetch(std::string_view symbol) {
    const std::string path = std::format("/public-quotes/bboquotes/instrument/{}", symbol);
#ifdef SWISSQUOTE_HAVE_OPENSSL
    auto res = std::visit([&](auto& client) { return client.Get(path); }, impl_->client);
#else
    auto res = impl_->client.Get(path);
#endif
    if (!res) {
        return std::unexpected(std::string("HTTP request failed: ") + httplib::to_string(res.error()));
    }

    HttpResponse out{
        .status = res->status,
        .body = std::vector<std::byte>(res->body.size()),
    };
    if (!res->body.empty()) {
        std::memcpy(out.body.data(), res->body.data(), res->body.size());
    }
    return out;
}

std::expected<HttpResponse, std::string> fetch_quotes(std::string_view symbol) {
    QuotesClient client;
    return client.fetch(symbol);
}

} // namespace swissquote::net
