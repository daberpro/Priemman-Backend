#pragma once

#include <src/handlers/user/authenticated_handler_base.hpp>

namespace priemman::handlers::admin {

class AdminUpgradeListHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-admin-upgrades";

    using AuthenticatedHandlerBase::AuthenticatedHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

class AdminUpgradeReviewHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-admin-upgrades-review";

    using AuthenticatedHandlerBase::AuthenticatedHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

class AdminUpgradeConfirmPaymentHandler final : public AuthenticatedHandlerBase {
public:
    static constexpr std::string_view kName = "handler-admin-upgrades-confirm-payment";

    using AuthenticatedHandlerBase::AuthenticatedHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;
};

}  // namespace priemman::handlers::admin
