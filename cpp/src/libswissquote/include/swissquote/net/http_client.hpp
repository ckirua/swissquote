#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace swissquote::net {

struct HttpResponse {
    int status{};
    std::vector<std::byte> body;
};

class QuotesClient {
public:
    QuotesClient();
    ~QuotesClient();

    QuotesClient(const QuotesClient&) = delete;
    QuotesClient& operator=(const QuotesClient&) = delete;
    QuotesClient(QuotesClient&&) noexcept;
    QuotesClient& operator=(QuotesClient&&) noexcept;

    [[nodiscard]] std::expected<HttpResponse, std::string> fetch(std::string_view symbol);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string quotes_url(std::string_view symbol);
[[nodiscard]] std::expected<HttpResponse, std::string> fetch_quotes(std::string_view symbol);

} // namespace swissquote::net
