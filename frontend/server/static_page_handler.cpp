#include "static_page_handler.hpp"
#include "session_middleware.hpp"

#include <optional>
#include <string_view>
#include <unordered_map>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/fs/blocking/read.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/yaml_config/yaml_config.hpp>

namespace priemman::frontend {

namespace {

std::string ContentTypeForPath(const std::string& path) {
    static const std::unordered_map<std::string_view, std::string_view> kTypes = {
        {".html", "text/html; charset=utf-8"},
        {".htm", "text/html; charset=utf-8"},
        {".js", "application/javascript"},
        {".mjs", "application/javascript"},
        {".css", "text/css"},
        {".json", "application/json"},
        {".map", "application/json"},
        {".svg", "image/svg+xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".avif", "image/avif"},
        {".ico", "image/x-icon"},
        {".txt", "text/plain; charset=utf-8"},
        {".xml", "application/xml"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".webmanifest", "application/manifest+json"},
    };

    const auto dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }

    const std::string_view ext = std::string_view{path}.substr(dot);
    if (const auto it = kTypes.find(ext); it != kTypes.end()) {
        return std::string{it->second};
    }
    return "application/octet-stream";
}

std::string UrlEncodeValue(const std::string& value) {
    static constexpr std::string_view kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (const unsigned char c : value) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~' || c == '/';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

// FIX #1: deteksi request RSC prefetch dari Next.js App Router
bool IsRscRequest(const userver::server::http::HttpRequest& request) {
    return !request.GetHeader("RSC").empty() ||
           request.GetHeader("Accept")
                   .find("text/x-component") != std::string::npos;
}

// FIX #2: sanitasi arg ?next= — cegah open-redirect dan loop ".txt"
std::string SafeNext(const userver::server::http::HttpRequest& request,
                     const std::string& fallback) {
    const auto raw = request.GetArg("next");
    if (!raw.empty() && raw.front() == '/' &&
        raw.find("..") == std::string::npos &&
        raw.find(".txt") == std::string::npos &&
        raw.find("_rsc") == std::string::npos) {
        return raw;
    }
    return fallback;
}

userver::fs::FileInfoWithDataConstPtr TryResolve(
    const userver::fs::FsCacheClient& fs,
    const std::string& rel
) {
    if (auto file = fs.TryGetFile(rel)) {
        return file;
    }
    return fs.TryGetFile("/" + rel);
}

std::string DashboardForRole(const std::string& role) {
    if (role == "admin") return "/dashboard-admin";
    if (role == "creator") return "/dashboard-creator";
    return "/dashboard";
}

}  // namespace

StaticPageHandler::StaticPageHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerBase(config, context),
      _fs(&context.FindComponent<userver::components::FsCache>(
               config["fs-cache-component"].As<std::string>())
               .GetClient()) {
    const auto favicon_path =
        config["favicon-path"].As<std::string>("../static/favicon.ico");
    try {
        _favicon_data = userver::fs::blocking::ReadFileContents(favicon_path);
        LOG_INFO() << "favicon loaded from " << favicon_path;
    } catch (const std::exception& ex) {
        LOG_WARNING() << "favicon not loaded from " << favicon_path
                      << ", falling back to dist file: " << ex.what();
    }
}

userver::yaml_config::Schema StaticPageHandler::GetStaticConfigSchema() {
    auto schema = HttpHandlerBase::GetStaticConfigSchema();
    if (!schema.properties.has_value()) {
        schema.properties.emplace();
    }
    schema.properties->emplace(
        "fs-cache-component",
        userver::yaml_config::SchemaPtr(
            userver::yaml_config::impl::SchemaFromString(R"(
type: string
description: Name of the fs-cache component that serves the static files
)")
        )
    );
    schema.properties->emplace(
        "favicon-path",
        userver::yaml_config::SchemaPtr(
            userver::yaml_config::impl::SchemaFromString(R"(
type: string
description: Path to the favicon.ico served at /favicon.ico, overrides the dist file
)")
        )
    );
    return schema;
}

std::string StaticPageHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context
) const {
    auto& response = request.GetHttpResponse();

    if (request.GetMethod() == userver::server::http::HttpMethod::kOptions) {
        response.SetStatus(userver::server::http::HttpStatus::kOk);
        return {};
    }

    const std::string path = request.GetRequestPath();

    const auto* identity =
        context.GetDataOptional<std::optional<database::SessionIdentity>>(kSessionDataKey);
    const bool logged_in = identity != nullptr && identity->has_value();
    const std::string role = logged_in ? identity->value().role : std::string{};

    const bool is_rsc = IsRscRequest(request);

    const auto redirect_to = [&response](const std::string& url) -> std::string {
        response.SetStatus(userver::server::http::HttpStatus::kFound);
        response.SetHeader(std::string_view{"Location"}, url);
        return {};
    };

    // FIX #3: untuk request RSC prefetch, JANGAN pernah redirect.
    // Balas status polos agar tidak ada chain redirect -> loop.
    // Next.js akan menangani 401 di sisi client.
    const auto guard_denied = [&](bool has_access) -> bool {
        if (has_access) {
            return false;
        }
        if (is_rsc) {
            response.SetStatus(logged_in
                ? userver::server::http::HttpStatus::kForbidden
                : userver::server::http::HttpStatus::kUnauthorized);
            return true;
        }
        return false;  // lanjut ke logika redirect normal di bawah
    };

    // FIX #4: "next" dibangun dari PATH saja (GetRequestPath()),
    // bukan GetUrl(), supaya ?_rsc=... tidak ikut.
    const std::string login_redirect =
        "/login?next=" + UrlEncodeValue(path);

    if ((path == "/login" || path == "/sign-in")) {
        if (logged_in) {
            if (is_rsc) {
                // Prefetch halaman login saat sudah login:
                // cukup balas redirect tanpa diproses ulang sebagai dokumen.
                return redirect_to(SafeNext(request, DashboardForRole(role)));
            }
            return redirect_to(SafeNext(request, DashboardForRole(role)));
        }
    } else if (path.starts_with("/admin") || path.starts_with("/dashboard-admin")) {
        if (guard_denied(logged_in)) return {};
        if (!logged_in) {
            return redirect_to(login_redirect);
        }
        if (role != "admin") {
            return redirect_to(DashboardForRole(role));
        }
    } else if (path.starts_with("/dashboard-creator")) {
        if (guard_denied(logged_in)) return {};
        if (!logged_in) {
            return redirect_to(login_redirect);
        }
        if (role != "creator") {
            return redirect_to(DashboardForRole(role));
        }
    } else if (path.starts_with("/dashboard")) {
        if (guard_denied(logged_in)) return {};
        if (!logged_in) {
            return redirect_to(login_redirect);
        }
        // FIX: creator/admin yang buka /dashboard dilempar ke dashboard miliknya
        if (role == "creator" || role == "admin") {
            return redirect_to(DashboardForRole(role));
        }
    }

    if (path == "/favicon.ico" && !_favicon_data.empty()) {
        response.SetContentType("image/x-icon");
        response.SetHeader(std::string_view{"Cache-Control"}, "public, max-age=3600");
        return _favicon_data;
    }

    userver::fs::FileInfoWithDataConstPtr file;
    std::string resolved;

    if (path == "/") {
        resolved = "index.html";
        file = TryResolve(*_fs, resolved);
    } else {
        const std::string rel = path.substr(1);
        for (const auto& candidate : {rel, rel + ".html", rel + "/index.html"}) {
            if (auto found = TryResolve(*_fs, candidate)) {
                file = found;
                resolved = candidate;
                break;
            }
        }
    }

    if (!file) {
        response.SetStatus(userver::server::http::HttpStatus::kNotFound);
        if (auto not_found = TryResolve(*_fs, "404.html")) {
            response.SetContentType("text/html; charset=utf-8");
            return not_found->data;
        }
        response.SetContentType("text/plain; charset=utf-8");
        return "404 Not Found";
    }

    if (path.starts_with("/_next/static/")) {
        response.SetHeader(
            std::string_view{"Cache-Control"}, "public, max-age=31536000, immutable");
    } else if (resolved.ends_with(".html")) {
        response.SetHeader(std::string_view{"Cache-Control"}, "no-cache");
    }

    response.SetContentType(ContentTypeForPath(resolved));
    return file->data;
}

}  // namespace priemman::frontend
