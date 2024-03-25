#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {

    using namespace std::literals;

    void SessionBase::Run() {
        // Вызываем метод Read, используя  executor stream_.
        // Таким образом вся работа со stream_ будет выполняться, используя его executor
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
    }

    void SessionBase::Read() { 
        // очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
        request_ = {};
        stream_.expires_after(30s);
        // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
        http::async_read(stream_, buffer_, request_,
                         // По окончании операции будет вызван метод OnRerad
                         beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
    }

    void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
        if(ec == http::error::end_of_stream) {
            // Нормальная ситуация - клиент закрыл соединение
            return Close();
        }
        if(ec) {
            return ReportError(ec, "read"sv);
        }
        std::string ip = stream_.socket().local_endpoint().address().to_string();
        HandleRequest(std::move(request_), std::move(ip));
    }

    void SessionBase::Close() {
        stream_.socket().shutdown(tcp::socket::shutdown_send);
    }

    void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        if(ec) {
            return ReportError(ec, "write"sv);
        }

        if(close) {
            // Семантика ответа требует закрыть соединение
            return Close();
        }

        // Считываем следующий запрос
        Read();
    }
}  // namespace http_server
