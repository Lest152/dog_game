#include "Players.h"
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <map>

namespace app {
static const int MILLISECONDS_IN_SECOND = 1000;

using namespace std::literals;

std::shared_ptr<Player> Players::Add(std::shared_ptr<model::Dog> dog, std::shared_ptr<model::GameSession> session) {
    Id player_id = Id{std::to_string(*dog->GetId()) + "_dogId_" + *session->GetMap().GetId() + "_mapId"};
    std::shared_ptr<Player> player = std::make_shared<Player>(session, dog);
    if(auto [it, inserted] = player_id_to_player_.emplace(player_id, player); !inserted) {
        throw std::invalid_argument("Player with id "s + *player_id + " already exists"s);
    }
    return player;
}

std::shared_ptr<Player> Players::DeletePlayer(std::shared_ptr<model::Dog> dog, std::shared_ptr<model::GameSession> session) {
    Id player_id = Id{std::to_string(*dog->GetId()) + "_dogId_" + *session->GetMap().GetId() + "_mapId"};
    if(player_id_to_player_.contains(player_id)) {
        auto player = player_id_to_player_[player_id];
        player_id_to_player_.erase(player_id);
        return player;
    }

    return nullptr;
}

std::shared_ptr<model::GameSession> Player::GetSession() const {
    return session_;
}

std::shared_ptr<model::Dog> Player::GetDog() const {
    return dog_;
}

Token PlayerTokens::GenerateToken() {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;

        std::uint64_t num1 = dist(generator1_);
        std::uint64_t num2 = dist(generator2_);

        std::stringstream token;
        token << std::hex << std::setw(16) << std::setfill('0') << num1 << std::setw(16) << std::setfill('0') << num2;
        return Token{ token.str() };
}

Token PlayerTokens::AddPlayer(std::shared_ptr<Player> playerPtr) {
    Token token = GenerateToken();
    if(auto [it, inserted] = token_to_player_.emplace(token, playerPtr); !inserted) {
        return AddPlayer(playerPtr);
    }
    return token;
}

void PlayerTokens::DeletePlayer(std::shared_ptr<Player> playerPtr) {
    for(auto& data : token_to_player_) {
        if(auto& [token, player] = data; playerPtr->GetId() == player->GetId()) {
            token_to_player_.erase(token);
            return;
        }
    }
}

std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(Token token) {
    if(token_to_player_.contains(token)) {
        return token_to_player_[token];
    }
    return nullptr;
}

model::Dog::Id Player::GetId() const {
    return dog_->GetId();
}

json::object Application::ConnectToGame(std::string user_name, std::string map_id) {
    // Добавляем собаку
    auto dog_id = model::DogsId::GetDogId();
    std::shared_ptr<model::Dog> dog = std::make_shared<model::Dog>(model::Dog::Id{dog_id}, user_name);
    // Получаем сессию
    auto sessionPtr =  game_.ConnectToSession(model::Map::Id{map_id}, dog, is_random_);
    // Создаем игрока
    auto playerPtr = players_->Add(dog, sessionPtr);
    std::string token = *tokens_->AddPlayer(playerPtr);
    size_t player_id = *playerPtr->GetId();
    // Генерируем ответ
    json::object obj( {{"authToken", token}, {"playerId", player_id}} );
    return obj;
}

json::object Application::GetPlayers(std::string _token) {
    Token token {_token};

    auto player  = tokens_->FindPlayerByToken(token);
    if(!player) {
        return json::object {{"code", "unknownToken"}, {"message", "Player token has not been found"}};
    }
    auto session = player->GetSession();
    auto dogs    = session->GetDogs();

    json::object obj;
    for(auto dog : *dogs) {
        json::object dogObj;
        dogObj["name"] = dog->GetName();
        obj[std::to_string(*dog->GetId())] = dogObj;
    }

    return obj;
}

json::array Application::GetRecords(int start, int max_items) {
    json::array result;
    auto records = use_cases_.GetRetiredPlayer(start, max_items);
    
    for(const auto& [name, score, play_time] : records) {
        json::object record_obj;
        record_obj["name"] = name.substr(1, name.length() - 2); // Убираем кавычки в имени ""
        record_obj["score"] = (int)score;
        record_obj["playTime"] = play_time;
        
        result.push_back(record_obj);
    }
    
    return result;
}


json::object Application::GetState(std::string _token) {
    json::object result;
    Token token {_token};

    auto player  = tokens_->FindPlayerByToken(token);
    if(!player) {
        return json::object {{"code", "unknownToken"}, {"message", "Player token has not been found"}};
    }
    auto session = player->GetSession();
    auto dogs    = session->GetDogs();

    // Получение информации об игроках
    {
        json::object players;
        json::object data;
        for(auto dog : *dogs) {
            json::object dogObj;
            json::array bagArray;
            
            auto pos   = dog->GetPos();
            auto speed = dog->GetSpeed();
            auto dir   = dog->GetDir();
    
            dogObj["pos"]   = boost::json::array{pos.x, pos.y};
            dogObj["speed"] = boost::json::array{speed.horizont, speed.vertical};
            dogObj["dir"]   = dir;

            auto itemsInBag = dog->GetBagObjects();
            for(auto item : itemsInBag) {
                json::object bagItem;
                bagItem["id"]   = item.first;
                bagItem["type"] = item.second;
                bagArray.push_back(bagItem);
            }
            dogObj["bag"] = std::move(bagArray);
            
            size_t score = dog->GetScore();
            dogObj["score"] = score;

            data[std::to_string(*dog->GetId())] = std::move(dogObj);
        }

        result["players"] = std::move(data);
    }

    // Получение списка потерянных предметов
    {
        json::object data;
        auto loot_objects = session->GetLootObjects();
        for(auto loot : loot_objects) {
            json::object lootObj;

            size_t id   = std::get<0>(loot);
            auto pos    = std::get<1>(loot);
            size_t type = std::get<2>(loot);

            lootObj["type"] = type;
            lootObj["pos"]  = boost::json::array{pos.x, pos.y};

            data[std::to_string(id)] = lootObj;
        }

        result["lostObjects"] = std::move(data);
    }

    return result;
}

json::object Application::Move(std::string _token, std::string_view dir) {
    Token token {_token};

    auto player  = tokens_->FindPlayerByToken(token);
    if(!player) {
        return json::object {{"code", "unknownToken"}, {"message", "Player token has not been found"}};
    }
    auto session = player->GetSession();
    auto dog     = player->GetDog();
    auto map     = session->GetMap();
    double speed = map.GetDogSpeed();

    dog->SetDir(dir, speed);
    
    return json::object{};
}

model::Game& Application::GetGameObj() {
    return game_;
}

void Application::Tick(std::chrono::milliseconds timer) {
    int delta = timer.count();
    Tick(delta);
}

void Application::Tick(const int delta) const {
    auto sessions = game_.GetSessions();
    for(auto session : sessions) {
        std::vector<collision_detector::Gatherer> gatherers;
        std::vector<collision_detector::Item> items;

        auto map   = session->GetMap();
        auto roads = map.GetRoads();
        auto dogs  = *session->GetDogs();

        for(auto dog : dogs) {
            // Расчет новой позиции   
            CalcNewPos(dog, roads, gatherers, delta);
            // Проверка неактивных пользователей
            CheckPlayerDisconnect(dog, delta);
        }

        // Генерирование потерянных предметов
        size_t loot_count   = session->LootCount();
        size_t looter_count = session->DogsCount();        

        size_t generate_count = game_.LootGenerate(std::chrono::milliseconds(delta), loot_count, looter_count);
        for(size_t i = 0; i < generate_count; i++) {
            session->AddLoot();
        }

        auto lost_objects = session->GetLootObjects();
        for(auto lost_object : lost_objects) {
            collision_detector::Item item;
            auto coord = std::get<1>(lost_object);
            item.position = geom::Point2D(coord.x, coord.y);
            item.width = 0.0;
            items.emplace_back(item);
        }

        auto offices = map.GetOffices();
        for(auto office : offices) {
            collision_detector::Item item;
            auto coord = office.GetPosition();
            item.position = geom::Point2D(coord.x, coord.y);
            item.width = 0.25;
            items.emplace_back(item);
        }

        // Обработка коллизий
        std::map<size_t, bool> uses_items;  // Отмечаем подобранные предметы
        auto events = FindGatherEvents(VectorItemGathererProvider{items, gatherers});
        for(auto event : events) {
            size_t gatherer_id = event.gatherer_id;
            size_t item_id     = event.item_id;

            bool is_lost_item = item_id < events.size(); // true - потерянный предмет, false - оффис
            auto dog = dogs[gatherer_id];

            // Подбираем потерянный предмет
            if(is_lost_item && dog->GetItemsCount() < map.GetBagCapacity() && !uses_items.contains(item_id)) {
                auto lost_object = lost_objects[item_id];

                size_t id   = std::get<0>(lost_object);
                size_t type = std::get<2>(lost_object);
                size_t score = map.GetScoreLootType(type);

                // Добавляем предмет в рюкзак
                dog->AddItem(id, type, score);
                uses_items[item_id] = true;

                // Удаляем предмет с карты
                session->DeliteLoot(id);
            }
            
            // Сдать все предметы на базу
            if(!is_lost_item) {
                dog->FreeItems();
            }
        }
    }
}

void Application::CalcNewPos(std::shared_ptr<model::Dog> dog, model::Map::Roads& roads, std::vector<collision_detector::Gatherer>& gatherers, const int delta) const {
    
    collision_detector::Gatherer gatherer;
    auto speed = dog->GetSpeed();
    auto pos   = dog->GetPos();
    gatherer.start_pos = geom::Point2D(pos.x, pos.y);
    gatherer.width = 0.3d;
    
    double x = pos.x + (speed.horizont * delta / MILLISECONDS_IN_SECOND);
    double y = pos.y + (speed.vertical * delta / MILLISECONDS_IN_SECOND);

    model::Coordinate cur_pos{pos.x, pos.y};  
    model::Coordinate new_pos{pos.x, pos.y};
    const double road_width = 0.4;        

    for(auto road : roads) {
        auto start = road.GetStart();
        auto end   = road.GetEnd();
        double tmp;
        if (start.x > end.x) {
            auto temp = start.x;
            start.x = end.x;
            end.x = temp;
        }
        if (start.y > end.y) {
            auto temp = start.y;
            start.y = end.y;
            end.y = temp;
        }
        
        if ((start.x - road_width) <= pos.x && (end.x + road_width) >= pos.x && (start.y - road_width) <= pos.y && (end.y + road_width) >= pos.y) {
            if (speed.horizont > 0) {
                tmp = new_pos.x;
                if(road.IsHorizontal()) {
                    new_pos.x = x < (end.x + road_width) ? x : (end.x + road_width);
                } else {
                    new_pos.x = x < (start.x + road_width) ? x : (start.x + road_width);
                }
                new_pos.x = new_pos.x > tmp ? new_pos.x : tmp;
            } else if (speed.horizont < 0) {
                tmp = new_pos.x;
                if(road.IsHorizontal()) {
                    new_pos.x = x > (start.x - road_width) ? x : (start.x - road_width);
                } else {
                    new_pos.x = x > (start.x - road_width) ? x : (start.x - road_width);
                }
                new_pos.x = new_pos.x < tmp ? new_pos.x : tmp;
            } else if (speed.vertical > 0) {
                tmp = new_pos.y;
                if(road.IsHorizontal()) {
                    new_pos.y = y < (start.y + road_width) ? y : (start.y + road_width);
                } else {
                    new_pos.y = y < (end.y + road_width) ? y : (end.y + road_width);
                }
                new_pos.y = new_pos.y > tmp ? new_pos.y : tmp;
            } else if (speed.vertical < 0) {
                tmp = new_pos.y;
                if(road.IsHorizontal()) {
                    new_pos.y = y > (start.y - road_width) ? y : (start.y - road_width);
                } else {
                    new_pos.y = y > (start.y - road_width) ? y : (start.y - road_width);
                }
                new_pos.y = new_pos.y < tmp ? new_pos.y : tmp;
            }
        }
    }

    if (cur_pos.x == new_pos.x && cur_pos.y == new_pos.y) {
        dog->SetSpeed(model::Speed{0.0, 0.0});
    }

    dog->SetPos(new_pos);
    gatherer.end_pos = geom::Point2D(new_pos.x, new_pos.y);
    gatherers.emplace_back(gatherer);
}

void Application::CheckPlayerDisconnect(std::shared_ptr<model::Dog> dog, const int delta) const {
    dog->AddPlayTime((double)delta / (double)MILLISECONDS_IN_SECOND);
    double stop_time = dog->GetStopTime();
    auto pos = dog->GetPos();
    if(stop_time >= game_.GetDogRetirementTime()) {
        double play_time = dog->GetPlayTime();
        use_cases_.AddRetiredPLayer(dog->GetName(), dog->GetScore(), play_time);
        auto sessions = game_.GetSessions();
        for(auto& session : sessions) {
            session->DeleteDog(dog);
            auto player = players_->DeletePlayer(dog, session);
            if(player) {
                tokens_->DeletePlayer(player);
            }
        }
    }
}

 const bool Application::IsTick() const {
    return is_tick_;
 }

}