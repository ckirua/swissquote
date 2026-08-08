#include "swissquote/net/http_client.hpp"

#include <cstring>
#include <format>

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

} // namespace

std::string quotes_url(std::string_view symbol) {
    return std::format("https://{}/public-quotes/bboquotes/instrument/{}", kHost, symbol);
}

struct QuotesClient::Impl {
#ifdef SWISSQUOTE_HAVE_OPENSSL
    httplib::SSLClient client{kHost, kPort};
#else
    httplib::Client client{kHost, kPort};
#endif

    Impl() {
#ifdef SWISSQUOTE_HAVE_OPENSSL
        client.enable_server_certificate_verification(true);
#endif
        client.set_connection_timeout(10, 0);
        client.set_keep_alive(true);
        client.set_read_timeout(30, 0);
    }
};

QuotesClient::QuotesClient() : impl_(std::make_unique<Impl>()) {}

QuotesClient::~QuotesClient() = default;

QuotesClient::QuotesClient(QuotesClient&&) noexcept = default;

QuotesClient& QuotesClient::operator=(QuotesClient&&) noexcept = default;

std::expected<HttpResponse, std::string> QuotesClient::fetch(std::string_view symbol) {
    const std::string path = std::format("/public-quotes/bboquotes/instrument/{}", symbol);
    auto res = impl_->client.Get(path);
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
