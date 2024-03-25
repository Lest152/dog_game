#pragma once
#include "http_server.h"
#include <boost/json.hpp>
#include "model.h"
#include "Players.h"

namespace http_handler {
    namespace beast = boost::beast;
    namespace sys   = boost::system;
    namespace http  = beast::http;
    namespace json  = boost::json;
    namespace fs    = std::filesystem;

    using StringResponse = http::response<http::string_body>;
    using StringRequest  = http::request<http::string_body>;
    using namespace std::literals;

    class ApiRequestHandler {
    public:
        explicit ApiRequestHandler(app::Application& app) 
            : app_{app} {}

        StringResponse operator()(const StringRequest& req);

    private:

        app::Application& app_;
        std::shared_ptr<app::PlayerTokens> tokens_;
        std::shared_ptr<app::Players>      players_;

        struct ContentType {
            ContentType() = delete;
            constexpr static std::string_view TEXT_HTML = "text/html"sv;
            constexpr static std::string_view JSON      = "application/json"sv;
        };

        // Создаёт StringResponse с заданными параметрами
        StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                        bool keep_alive,
                                        std::string_view content_type);

        StringResponse HandleRequest(const StringRequest& req);
    };
}