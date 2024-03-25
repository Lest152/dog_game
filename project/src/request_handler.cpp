#include "request_handler.h"

namespace http_handler {

    // Конвертируем из 16 в 10 ричную систему счисления
    int convert_16to10(char chr){
        if (chr >= '0' && chr <= '9')
            return chr - '0';
        if (chr >= 'A' && chr <= 'F')
            return chr - 'A' + 10;
        if (chr >= 'a' && chr <= 'f')
            return chr - 'a' + 10;
        return -1;
    }

    // Возвращает true, если каталог p содержится внутри base_path.
    bool IsSubPath(fs::path path, fs::path base) {
        // Приводим оба пути к каноническому виду (без . и ..)
        path = fs::weakly_canonical(path);
        base = fs::weakly_canonical(base);

        // Проверяем, что все компоненты base содержатся внутри path
        for(auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if(p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    }

    FileResponse RequestHandler::HandleRequest(StringRequest&& req) {
        std::string target = URL_decoder(req.target().substr(1));
        if(target.empty()) {
            target = "index.html";
        }
        fs::path path = fs::weakly_canonical(static_path_ / target);
        // Приведение раширения к нижнему регистру
        {
            std::string extension = path.extension().c_str();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
            path.replace_extension(extension);
        }
        FileResponse res;
        res.result(http::status::ok);
        http::file_body::value_type file;

        if(!fs::exists(path)) {
            path = fs::weakly_canonical(static_path_ / "NotFound.txt");
            res.result(http::status::not_found);
            res.insert(http::field::content_type, "text/plain"sv);
        } else if(!IsSubPath(path, static_path_)) {
            path = fs::weakly_canonical(static_path_ / "NotFound.txt");
            res.result(http::status::bad_request);
            res.insert(http::field::content_type, "text/plain"sv);
        } else if(std::string_view ex = path.extension().c_str(); ContentTypeOfExtension.contains(ex)) {
            res.insert(http::field::content_type, ContentTypeOfExtension[ex]);
        } else {
            res.insert(http::field::content_type, "application/octet-stream"sv);
        }

        if(sys::error_code ec; file.open(path.c_str(), beast::file_mode::read, ec), ec) {
            std::cout << "Failed to open file"sv << path.c_str() << std::endl;
            return res;
        }

        res.body() = std::move(file);
        // Метод prepare_payload заполняет заголовки Content-Length и Transfer-Encoding
        // в зависимости от свойств тела сообщения
        res.prepare_payload();

        return res;
    }

    std::string RequestHandler::URL_decoder(std::string_view code) {
        std::string decode;

        auto decoder = [this](const char* c) {
            int i = convert_16to10(c[0]) * 16 + convert_16to10(c[1]);
            return static_cast<char>(i);
        };

        for (size_t i = 0; i < code.size(); i++) {
            if(code[i] == '%') {
                const char c[] = { code[i + 1], code[i + 2] };
                decode += decoder(c);
                i += 2;
            } else if (code[i] == '+') {
                decode += ' ';
            } else {
                decode += code[i];
            }
        }

        return decode;
    }

}  // namespace http_handler
