#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <userver/engine/mutex.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/middlewares/http_middleware_base.hpp>

namespace priemman::middlewares {

struct RateLimiterSharedState {
    userver::engine::Mutex mutex;
    std::unordered_map<std::string, std::deque<std::int64_t>> hits;
};

class RateLimiterMiddleware final : public userver::server::middlewares::HttpMiddlewareBase {
public:
    static constexpr std::string_view kName = "rate-limiter-middleware";

    RateLimiterMiddleware(
        std::size_t requests_per_minute,
        std::vector<std::string> exempt_path_prefixes,
        std::shared_ptr<RateLimiterSharedState> state
    );

protected:
    void HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    bool TryConsume(const std::string& ip) const;
    bool IsExempt(const std::string& path) const;

    const std::size_t _limit;
    const std::vector<std::string> _exempt_prefixes;
    std::shared_ptr<RateLimiterSharedState> _state;
};

class RateLimiterFactory final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
public:
    static constexpr std::string_view kName = RateLimiterMiddleware::kName;

    RateLimiterFactory(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    userver::yaml_config::Schema GetMiddlewareConfigSchema() const override;

    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Create(
        const userver::server::handlers::HttpHandlerBase& handler,
        userver::yaml_config::YamlConfig middleware_config
    ) const override;

    std::shared_ptr<RateLimiterSharedState> _state;
    std::size_t _default_limit;
    std::vector<std::string> _default_exempt_prefixes;
};

}  // namespace priemman::middlewares
