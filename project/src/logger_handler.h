#pragma once

#include <boost/log/trivial.hpp>     // для BOOST_LOG_TRIVIAL
#include <boost/log/core.hpp>        // для logging::core
#include <boost/log/expressions.hpp> // для выражения, задающего фильтр
#include <boost/log/utility/setup/file.hpp> // для логирования в файл
#include <boost/log/utility/setup/common_attributes.hpp>  // logging::add_common_attributes()
#include <boost/log/utility/setup/console.hpp> // logging::add_console_log
#include <boost/date_time.hpp>  // для вывода момента времени в логе
#include <boost/log/utility/manipulators/add_value.hpp>  // Для задания атрибутов при логировании

#include <chrono>

#include "request_handler.h"

namespace http_handler {

    using namespace std::literals;
    namespace logging = boost::log;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;

    BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
    BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

    template <class SomeRequestHandler>
    class LoggingRequestHandler {

        void LogRequest(const StringRequest&& r, std::string&& ip) {
            std::string_view method = r.method_string();
            json::value custom_data{
                {"ip"s,     ip},
                {"URI"s,    r.target()},
                {"method"s, method}
            };
            
            WriteLog(custom_data, "request received");
        }

        void LogResponse(std::tuple<int, std::string>&& info, std::string&& ip, unsigned t) {

            json::value custom_data {
                {"ip"s,            ip},
                {"response_time"s, t},
                {"code"s,          std::get<0>(info)},
                {"content_type"s,  std::get<1>(info)}
            };

            WriteLog(custom_data, "response sent");
        }

        void WriteLog(json::value data, std::string message) {
            json::value custom_data {
                {"timestamp"s, getCurrentTime()},
                {"data"s,      data},
                {"message"s,   message}
            };

            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, custom_data);
        }

        static std::string getCurrentTime() {
            boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
            std::string iso8601 = to_iso_extended_string(now);
            return iso8601;
        }
    
    public:
        explicit LoggingRequestHandler(SomeRequestHandler& decorated, unsigned port, std::string ip)
            : decorated_{decorated} {
                logging::add_common_attributes();

                logging::formatter formatter =
                    logging::expressions::stream << additional_data;

                logging::add_console_log(
                    std::cout,
                    logging::keywords::auto_flush = true
                    )->set_formatter(formatter);
                    

                json::value data {{"port"s, port}, {"address", ip}};
                WriteLog(data, "server started"s);
            }

        ~LoggingRequestHandler() {
            json::value data {{"code"s, 0}};
            WriteLog(data, "server exited"s);
        }

        template <typename Body, typename Allocator, typename Send>
        void operator()(std::string&& ip, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            LogRequest(std::move(req), std::move(ip));
            auto start = std::chrono::steady_clock::now();
            std::tuple<int, std::string> res_info = decorated_(std::move(req), std::move(send));
            auto end = std::chrono::steady_clock::now();
            auto diff = end - start;
            LogResponse(std::move(res_info), std::move(ip), std::chrono::duration<double, std::milli>(diff).count());
        }

    private:
        SomeRequestHandler& decorated_;
    };

} // namespace http_handler