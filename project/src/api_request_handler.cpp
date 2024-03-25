#include "api_request_handler.h"

#include <iostream>
namespace http_handler {

    std::string_view maps_target    = "/api/v1/maps"sv;
    std::string_view join_target    = "/api/v1/game/join"sv;
    std::string_view players_target = "/api/v1/game/players"sv;
    std::string_view state_target   = "/api/v1/game/state"sv;
    std::string_view action_target  = "/api/v1/game/player/action"sv;
    std::string_view tick_target    = "/api/v1/game/tick"sv;
    std::string_view record_target  = "/api/v1/game/records"sv;

    // Получить id карты
    auto getMapId(json::value& value) {
        return value.get_object().at("id");
    }

    // Получить название карты
    auto getMapName(json::value& value) {
        return value.get_object().at("name");
    }

    // Получить список карт
    json::array getArrayMaps(auto& maps) {
        json::array arr_maps;
        for(auto it = maps.begin(); it < maps.end(); it++) {
            auto map = *it;
            auto value = json::parse(map.GetConfig());
            json::object obj ( {
                                {"id",   getMapId(value)},
                                {"name", getMapName(value)}
                            } );
            arr_maps.emplace_back(obj);
        }

        return std::move(arr_maps);
    }

    StringResponse ApiRequestHandler::operator()(const StringRequest& req) {
            return HandleRequest(req);
        }

    StringResponse ApiRequestHandler::MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                                         bool keep_alive,
                                                         std::string_view content_type = ContentType::TEXT_HTML) {
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(keep_alive);
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    StringResponse ApiRequestHandler::HandleRequest(const StringRequest& req) {
        const auto text_response = [&req, this](http::status status, std::string_view text) {
            return MakeStringResponse(status, text, req.version(), req.keep_alive(), ContentType::JSON);
        };

        const auto createErrorResponse = [](std::string code, std::string message) {
            return json::object {{"code", code}, {"message", message}};
        };

        if(req.target() == maps_target &&
           req.method() == http::verb::get) {
            json::array arr_maps = getArrayMaps(app_.GetGameObj().GetMaps());
            return text_response(http::status::ok, json::serialize(arr_maps));
        }
        if(req.target().size() > 13                        &&
           req.target().substr(0, 13) == "/api/v1/maps/"sv) {
            if(req.method() != http::verb::get &&
               req.method() != http::verb::head) {
                const auto obj = createErrorResponse("invalidMethod", "Only POST method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "GET, HEAD");
                return response;
            }
            model::Map::Id id{std::string(req.target().substr(13))};
            auto map = app_.GetGameObj().FindMap(id);
            if(!map) {
                const auto obj = createErrorResponse("mapNotFound", "Map not found");
                return text_response(http::status::not_found, json::serialize(obj));
            }
            auto value = json::parse(map->GetConfig());
            return text_response(http::status::ok, json::serialize(value));
        }
        if(req.target() == join_target) {
            if(req.method() != http::verb::post) {
                const auto obj = createErrorResponse("invalidMethod", "Only POST method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "POST");
                return response;
            }
            json::value body = json::parse(req.body());
            std::string userName;
            try {
                userName = json::serialize(body.at("userName"));
                if(userName.empty() || userName == "" || userName == "\"\"")
                    throw std::invalid_argument("Invalid name");
            } catch(const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Invalid name");
                return text_response(http::status::bad_request, json::serialize(obj));
            } catch(boost::wrapexcept<std::invalid_argument>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Invalid name");
                return text_response(http::status::bad_request, json::serialize(obj));
            }
            std::string mapId;
            try {
                mapId = json::serialize(body.at("mapId"));  
                mapId.erase(std::remove(mapId.begin(), mapId.end(), '"'), mapId.end()); // Убираем ковычки
            } catch(const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Invalid map");
                return text_response(http::status::bad_request, json::serialize(obj));
            } catch(boost::wrapexcept<std::invalid_argument>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Invalid map");
                return text_response(http::status::bad_request, json::serialize(obj));
            }
            if(!app_.GetGameObj().FindMap(model::Map::Id{mapId})) {
                const auto obj = createErrorResponse("mapNotFound", "Map not found");
                return text_response(http::status::not_found, json::serialize(obj));
            }
            json::object obj = app_.ConnectToGame(userName, mapId);
            return text_response(http::status::ok, json::serialize(obj));
        }
        if(req.target() == players_target) {
            if(req.method() != http::verb::get &&
               req.method() != http::verb::head) {
                const auto obj = createErrorResponse("invalidMethod", "Only POST method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "GET, HEAD");
                return response;
            }
            std::string_view authorizationValue;
            try {
                authorizationValue = req.at(http::field::authorization);
                if(authorizationValue.npos == authorizationValue.find("Bearer") ||
                   authorizationValue.size() != (32 + 7)) { // 32 - длина токена + 7 - длина слова Bearer + пробел
                    throw std::invalid_argument("Invalid token");
                }
            } catch (const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidToken", "Invalid token");
                return text_response(http::status::unauthorized, json::serialize(obj));
            } catch (const std::invalid_argument& e) {
                const auto obj = createErrorResponse("invalidToken", "Invalid token");
                return text_response(http::status::unauthorized, json::serialize(obj));
            }
            std::string_view token = authorizationValue.substr(7); // 7 - длина слова Bearer + пробел
            json::object obj = app_.GetPlayers(std::string {token});
            auto status = http::status::ok;
            if(auto it = obj.find("code"); it != obj.end() && obj["code"] == "unknownToken") {
                status = http::status::unauthorized;
            }
            return text_response(status, json::serialize(obj));
        }
        if(req.target() == state_target) {
            if(req.method() != http::verb::get &&
               req.method() != http::verb::head) {
                const auto obj = createErrorResponse("invalidMethod", "Only POST method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "GET, HEAD");
                return response;
            }
            std::string_view authorizationValue;
            try {
                authorizationValue = req.at(http::field::authorization);
                if(authorizationValue.npos == authorizationValue.find("Bearer") ||
                   authorizationValue.size() != (32 + 7)) { // 32 - длина токена + 7 - длина слова Bearer + пробел
                    throw std::invalid_argument("Invalid token");
                }
            } catch (const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidToken", "Invalid token");
                return text_response(http::status::unauthorized, json::serialize(obj));
            } catch (const std::invalid_argument& e) {
                const auto obj = createErrorResponse("invalidToken", "Invalid token");
                return text_response(http::status::unauthorized, json::serialize(obj));
            }
            std::string_view token = authorizationValue.substr(7); // 7 - длина слова Bearer + пробел
            json::object obj = app_.GetState(std::string {token});
            auto status = http::status::ok;
            if(auto it = obj.find("code"); it != obj.end() && obj["code"] == "unknownToken") {
                status = http::status::unauthorized;
            }
            return text_response(status, json::serialize(obj));
        }
        if(req.target() == action_target) {
            if(req.method() != http::verb::post) {
                const auto obj = createErrorResponse("invalidMethod", "Only POST method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "POST");
                return response;
            }
            std::string_view authorizationValue;
            try {
                authorizationValue = req.at(http::field::authorization);
                if(authorizationValue.npos == authorizationValue.find("Bearer") ||
                   authorizationValue.size() != (32 + 7)) { // 32 - длина токена + 7 - длина слова Bearer + пробел
                    throw std::invalid_argument("Invalid token");
                }
            } catch (const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidToken", "Invalid token");
                return text_response(http::status::unauthorized, json::serialize(obj));
            } catch (const std::invalid_argument& e) {
                const auto obj = createErrorResponse("invalidToken", "Invalid token");
                return text_response(http::status::unauthorized, json::serialize(obj));
            }
            std::string_view token = authorizationValue.substr(7); // 7 - длина слова Bearer + пробел
            std::string body = req.body();
            json::value json = json::parse(body);
            std::string_view dir;
            try {
                dir = json.at("move").as_string();
            } catch(const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Failed to parse action");
                return text_response(http::status::bad_request, json::serialize(obj));
            } catch(boost::wrapexcept<std::invalid_argument>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Failed to parse action");
                return text_response(http::status::bad_request, json::serialize(obj));
            }
            if(auto it = req.find(http::field::content_type); it == req.end() || std::string_view(it->value()) != ContentType::JSON) {
                const auto obj = createErrorResponse("invalidArgument", "Invalid content type");
                return text_response(http::status::bad_request, json::serialize(obj));
            }
            json::object obj = app_.Move(std::string {token}, dir);
            auto status = http::status::ok;
            if(auto it = obj.find("code"); it != obj.end() && obj["code"] == "unknownToken") {
                status = http::status::unauthorized;
            }
            return text_response(status, json::serialize(obj));
        }
        if(req.target() == tick_target) {
            if(app_.IsTick()) {
                const auto obj = createErrorResponse("badRequest", "Invalid endpoint");
                return text_response(http::status::bad_request, json::serialize(obj));
            }
            if(req.method() != http::verb::post) {
                const auto obj = createErrorResponse("invalidMethod", "Only POST method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "POST");
                return response;
            }
            json::value json;
            if(!req.body().empty()) {
                std::string body = req.body();
                json = json::parse(body);
            }
            int delta;
            try {
                delta = json.at("timeDelta").as_int64();
            } catch(const boost::wrapexcept<std::out_of_range>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Failed to parse tick request JSON");
                return text_response(http::status::bad_request, json::serialize(obj));
            } catch(boost::wrapexcept<std::invalid_argument>& e) {
                const auto obj = createErrorResponse("invalidArgument", "Failed to parse tick request JSON");
                return text_response(http::status::bad_request, json::serialize(obj));
            }

            app_.Tick(delta);
            return text_response(http::status::ok, json::serialize(json::object{}));
        }
        const int record_target_substring_len = 20;
        if(req.target().size() >= record_target_substring_len  &&
           req.target().substr(0, record_target_substring_len) == record_target) {
            const int MAX_RECORDS_IN_QUERY = 100;

            if(req.method() != http::verb::get) {
                const auto obj = createErrorResponse("invalidMethod", "Only GET method is expected");
                auto response = text_response(http::status::method_not_allowed, json::serialize(obj));
                response.set(http::field::allow, "GET");
                return response;
            }

            int start = 0;
            int max_items = 100;
            std::string body_string(req.target());  // Преобразование std::string_view в std::string

            size_t start_pos = body_string.find("start=");
            if (start_pos != std::string::npos) {
                size_t start_val_pos = start_pos + 6; // Позиция значения start
                size_t end_val_pos = body_string.find('&', start_val_pos); // Позиция окончания значения start
                std::string start_val = body_string.substr(start_val_pos, end_val_pos - start_val_pos); // Значение start
                start = std::stoi(start_val);
            }

            size_t max_items_pos = body_string.find("maxItems=");
            if (max_items_pos != std::string::npos) {
                size_t max_items_val_pos = max_items_pos + 9; // Позиция значения maxItems
                std::string max_items_val = body_string.substr(max_items_val_pos); // Значение maxItems
                max_items = std::stoi(max_items_val);
                if(max_items > MAX_RECORDS_IN_QUERY) {
                    const auto obj = createErrorResponse("badRequest", "Bad request");
                    return text_response(http::status::bad_request, json::serialize(obj));
                }
            }

            auto obj = app_.GetRecords(start, max_items);
            return text_response(http::status::ok, json::serialize(obj));
        }
        
        const auto obj = createErrorResponse("badRequest", "Bad request");
        return text_response(http::status::bad_request, json::serialize(obj));
    }
}