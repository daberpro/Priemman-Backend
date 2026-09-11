#include "rate_limiter.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string_view>

#include <proto/common.pb.h>
#include <userver/components/component_config.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/yaml_config/yaml_config.hpp>

namespace priemman::middlewares {

namespace {

constexpr std::chrono::milliseconds kWindow{60000};

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        userver::utils::datetime::SteadyClock().now().time_since_epoch()
    ).count();
}

std::string RateLimitedBody() {
    priemman::v1::Result r;
    r.set_is_error(true);
    r.set_message("Too many requests, slow down");
    r.mutable_error_detail()->set_code("RATE_LIMITED");
    r.mutable_error_detail()->set_message("Too many requests, slow down");
    return r.SerializeAsString();
}

}  // namespace

RateLimiterMiddleware::RateLimiterMiddleware(
    std::size_t requests_per_minute,
    std::vector<std::string> exempt_path_prefixes,
    std::shared_ptr<RateLimiterSharedState> state
)
    : _limit(requests_per_minute == 0 ? 60 : requests_per_minute),
      _exempt_prefixes(std::move(exempt_path_prefixes)),
      _state(std::move(state)) {
}

bool RateLimiterMiddleware::IsExempt(const std::string& path) const {
    return std::ranges::any_of(
        _exempt_prefixes,
        [&path](const std::string& prefix) {
            return !prefix.empty() && path.starts_with(prefix);
        }
    );
}

bool RateLimiterMiddleware::TryConsume(const std::string& ip) const {
    const auto now = NowMs();
    const auto window_start = now - kWindow.count();

    std::lock_guard lock(_state->mutex);
    auto& hits = _state->hits[ip];

    while (!hits.empty() && hits.front() <= window_start) {
        hits.pop_front();
    }

    if (hits.size() >= _limit) {
        return false;
    }

    hits.push_back(now);
    return true;
}

void RateLimiterMiddleware::HandleRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context
) const {
    if (request.GetMethod() == userver::server::http::HttpMethod::kOptions) {
        Next(request, context);
        return;
    }

    if (IsExempt(request.GetRequestPath())) {
        Next(request, context);
        return;
    }

    const std::string ip = request.GetRemoteAddress().PrimaryAddressString();
    if (TryConsume(ip)) {
        Next(request, context);
        return;
    }

    const auto now = NowMs();
    long retry_after = 1;
    {
        std::lock_guard lock(_state->mutex);
        const auto it = _state->hits.find(ip);
        if (it != _state->hits.end() && !it->second.empty()) {
            retry_after = std::max<long>(
                1, (kWindow.count() - (now - it->second.front()) + 999) / 1000);
        }
    }

    auto& response = request.GetHttpResponse();
    response.SetStatus(userver::server::http::HttpStatus::kTooManyRequests);
    response.SetContentType("application/x-protobuf");
    response.SetHeader(std::string_view{"Retry-After"}, std::to_string(retry_after));
    response.SetData(RateLimitedBody());
}

RateLimiterFactory::RateLimiterFactory(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpMiddlewareFactoryBase(config, context),
      _state(std::make_shared<RateLimiterSharedState>()),
      _default_limit(config["requests-per-minute"].As<std::size_t>(60)),
      _default_exempt_prefixes(
          config["exempt-path-prefixes"].As<std::vector<std::string>>({})
      ) {
}

userver::yaml_config::Schema RateLimiterFactory::GetStaticConfigSchema() {
    return userver::yaml_config::impl::SchemaFromString(R"(
type: object
description: Rate limiter middleware config
additionalProperties: false
properties:
    requests-per-minute:
        type: integer
        description: Max requests per IP per minute
    exempt-path-prefixes:
        type: array
        description: Path prefixes that bypass the rate limiter
        items:
            type: string
            description: Path prefix
)");
}

userver::yaml_config::Schema RateLimiterFactory::GetMiddlewareConfigSchema() const {
    return GetStaticConfigSchema();
}

std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> RateLimiterFactory::Create(
    const userver::server::handlers::HttpHandlerBase&,
    userver::yaml_config::YamlConfig middleware_config
) const {
    const auto limit = middleware_config["requests-per-minute"].As<std::size_t>(_default_limit);
    const auto exempt = middleware_config["exempt-path-prefixes"].As<std::vector<std::string>>(_default_exempt_prefixes);
    return std::make_unique<RateLimiterMiddleware>(limit, exempt, _state);
}

}  // namespace priemman::middlewares
