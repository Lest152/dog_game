#pragma once
#include "http_server.h"
#include "api_request_handler.h"
#include "model.h"
#include <boost/json.hpp>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <variant>
#include <utility>

namespace http_handler {
namespace beast = boost::beast;
namespace sys   = boost::system;
namespace http  = beast::http;
namespace json  = boost::json;
namespace fs    = std::filesystem;
namespace net   = boost::asio;
using namespace std::literals;

// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;
// Ответ, тело которого представлено в виде файла
using FileResponse = http::response<http::file_body>;

using Strand = net::strand<net::io_context::executor_type>;

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(app::Application& app, fs::path path, Strand api_strand)
        : app_{app}
        , static_path_{fs::canonical(path)}
        , api_strand_{api_strand} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    std::tuple<int, std::string> operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        int code = -1;
        std::string content_type = "null";
        if(req.target().substr(0, 5).compare("/api/"sv) != 0) {
            FileResponse res = HandleRequest(std::forward<decltype(req)>(req));
            code = res.result_int();
            content_type = res.at(http::field::content_type);
            send(res);
        } else {
            auto handle = [self = shared_from_this(), send, req = std::forward<decltype(req)>(req), &code, &content_type] {
                //running_in_this_thread() - возвращает true, если текущий поток выполняет функцию, отправленную в strand через post, dispatch или defer
                assert(self->api_strand_.running_in_this_thread());
                StringResponse res = self->api_handler_(req);
                code = res.result_int();
                content_type = res.at(http::field::content_type);
                send(res);
            };
            
            net::dispatch(api_strand_, handle);
        }

        return std::make_tuple(code, content_type);
    }

private:
    app::Application& app_;
    fs::path    static_path_;  // Путь со статическими файлами
    std::shared_ptr<app::PlayerTokens> tokens_;
    std::shared_ptr<app::Players>      players_;
    ApiRequestHandler api_handler_{app_}; // Обработчик REST API
    Strand api_strand_;

    // Значение заголовка Content-Type в зависимости от типа файла
    std::unordered_map<std::string_view, std::string_view> ContentTypeOfExtension 
                                                            {
                                                                {".html"sv, "text/html"sv},
                                                                {".htm"sv,  "text/html"sv},
                                                                {".css"sv,  "text/css"sv},
                                                                {".txt"sv,  "text/plain"sv},
                                                                {".js"sv,   "text/javascript"sv},
                                                                {".json"sv, "application/json"},
                                                                {".xml"sv,  "application/xml"sv},
                                                                {".png"sv,  "image/png"sv},
                                                                {".jpg"sv,  "image/jpeg"sv},
                                                                {".jpe"sv,  "image/jpeg"sv},
                                                                {".jpeg"sv, "image/jpeg"sv},
                                                                {".gif"sv,  "image/gif"sv},
                                                                {".bmp"sv,  "image/bmp"sv},
                                                                {".ico"sv,  "image/vnd.microsoft.icon"sv},
                                                                {".tiff"sv, "image/tiff"sv},
                                                                {".tif"sv,  "image/tiff"sv},
                                                                {".svg"sv,  "image/svg+xml"sv},
                                                                {".svgz"sv, "image/svg+xml"sv},
                                                                {".mp3"sv,  "audio/mpeg"sv}
                                                            };

    FileResponse HandleRequest(StringRequest&& req);

    // URL декодер
    std::string URL_decoder(std::string_view code);
    };

}  // namespace http_handler
