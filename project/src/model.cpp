#include "model.h"

#include <stdexcept>
#include <iostream>

namespace model {
using namespace std::literals;

bool operator<=(const Coordinate& coord, const Point& point) {
    return coord.x < point.x && coord.y < point.y;
}

bool operator>=(const Coordinate& coord, const Point& point) {
    return coord.x > point.x && coord.y > point.y;
}

void LostObjects::AddObject(const Coordinate& coord, size_t type) {
    objects_.emplace_back(std::make_tuple(id_++, coord, type));
}

void LostObjects::DeleteObject(size_t id) {
    objects_.erase(std::remove_if(objects_.begin(), objects_.end(), [id](const Object& obj) {
        return std::get<0>(obj) == id;
    }), objects_.end());
}

size_t LostObjects::GetCountObjects() const {
    return objects_.size();
}

std::vector<LostObjects::Object> LostObjects::GetObjects() const {
    return objects_;
}

void Bag::AddObject(size_t id, size_t type) {
    objects_.emplace_back(std::make_pair(id, type));
}

void Bag::FreeBag() {
    objects_.clear();
}

std::vector<Bag::Object> Bag::GetBag() const {
    return objects_;
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Map::SetDogSpeed(double speed) {
    dog_speed_ = speed;
}

void Map::SetBagCapacity(int capacity) {
    bag_capacity_ = capacity;
}

int Map::GetBagCapacity() const {
    return bag_capacity_;
}

size_t Map::GetLootTypesCount() const {
    return loot_types_.GetLootCount();
}

size_t Map::GetScoreLootType(size_t type) const {
    return loot_types_.GetScore(type);
}

size_t DogsId::GetDogId() {
    return id_++;
}

size_t SessionId::GetId() {
    return id_++;
}

Dog::Id Dog::GetId() const {
    return id_;
}

std::string Dog::GetName() const {
    return name_;
}

Coordinate Dog::GetPos() const {
    return pos_;
}

std::string Dog::GetDir() const {
    return dir_;
}

Speed Dog::GetSpeed() const {
    return speed_;
}

std::vector<Bag::Object> Dog::GetBagObjects() const {
    return bag_.GetBag();
}

size_t Dog::GetScore() const {
    return score_;
}

void Dog::SetPos(const Coordinate& pos) {
    pos_ = pos;
}

void Dog::SetDir(std::string_view dir, double speed) {
    dir_ = dir;

    if(dir_ == "L"sv) {
        speed_ = Speed{-speed, 0.0};
    } else if(dir_ == "R"sv) {
        speed_ = Speed{speed, 0.0};
    } else if(dir_ == "U"sv) {
        speed_ = Speed{0.0, -speed};
    } else if(dir_ == "D"sv) {
        speed_ = Speed{0.0, speed};
    } else if(dir_ == ""sv) {
        speed_ = Speed{0.0, 0.0};
    }

    if (dir_ != ""sv)
        is_move_ = true;
}

void Dog::SetSpeed(const Speed& speed) {
    speed_ = speed;
}

void Dog::AddItem(size_t id, size_t type, size_t score) {
    ++items_count_;
    score_ += score;
    bag_.AddObject(id, type);
}

void Dog::FreeItems() {
    items_count_ = 0;
    bag_.FreeBag();
}

int Dog::GetItemsCount() const {
    return items_count_;
}

void Dog::AddPlayTime(const double t) {
    play_time_ += t;
    if(!is_move_ && speed_.horizont == 0 && speed_.vertical == 0) {
        stop_time_ += t;
    } else {
        stop_time_ = 0;
    }

    is_move_ = false;
}

double Dog::GetStopTime() const {
    return stop_time_;
}

double Dog::GetPlayTime() const {
    return play_time_;
}

GameSession::Id GameSession::GetId() const {
    return id_;
}

std::shared_ptr<GameSession::Dogs> GameSession::GetDogs() const {
    return std::make_shared<Dogs>(dogs_);
}

int GetRandomInt(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(min, max);

    return dist(gen);
}

double GetRandomDouble(double min, double max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(min, max);

    double rand = dist(gen);
    return std::round(rand * 10) / 10;
}

Coordinate GameSession::GetRandomCoordinate() const {
    auto roads = map_.GetRoads();
    int random = GetRandomInt(0, roads.size() - 1);
    auto road  = roads[random];
    Point start = road.GetStart();
    Point end   = road.GetEnd();
    double x, y;
    if(road.IsHorizontal()) {
        x = GetRandomDouble(start.x, end.x);
        y = start.y;
    } else {
        x = start.x;
        y = GetRandomDouble(start.y, end.y);
    }

    return Coordinate{x, y};
}

void GameSession::AddDog(std::shared_ptr<Dog> dogPtr, bool is_random) {
    Coordinate coord;
    // Размещаем пса на рандомной позиции дороги
    if(is_random) {
        coord = GetRandomCoordinate();
    } else {   // Размещаем пса в начальной точке координат
        auto roads = map_.GetRoads();
        coord.x = roads[0].GetStart().x;
        coord.y = roads[0].GetStart().y;
    }

    dogPtr->SetPos(coord);
    dogs_.emplace_back(dogPtr);
}

void GameSession::DeleteDog(std::shared_ptr<Dog> dogPtr) {
    for(auto it = dogs_.begin(); it != dogs_.end(); it++) {
        auto dog = *it;
        if(dogPtr->GetId() == dog->GetId()) {
            dogs_.erase(it);
            return;
        }
    }
}

void GameSession::AddLoot() {
    size_t     type  = GetRandomInt(0, map_.GetLootTypesCount() - 1);
    Coordinate coord = GetRandomCoordinate();

    lost_objects_.AddObject(coord, type);
}

void GameSession::DeliteLoot(size_t id) {
    lost_objects_.DeleteObject(id);
}

std::vector<LostObjects::Object> GameSession::GetLootObjects() const {
    return lost_objects_.GetObjects();
}

size_t GameSession::DogsCount() const {
    return dogs_.size();
}

size_t GameSession::LootCount() const {
    return lost_objects_.GetCountObjects();
}

const Map GameSession::GetMap() const {
    return map_;
}

void Game::SetDogRetirementTime(const float dog_retirement_time) {
    dog_retirement_time_ = dog_retirement_time;
}

const float Game::GetDogRetirementTime() const {
    return dog_retirement_time_;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Map::Id& Map::GetId() const noexcept {
        return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

const std::string& Map::GetConfig() const noexcept {
    return config_;
}

double Map::GetDogSpeed() const {
    return dog_speed_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

void Game::AddSession(std::shared_ptr<GameSession> session) {
    const size_t index = sessions_.size();
    if (auto [it, inserted] = map_id_to_session_index_.emplace(session->GetMap().GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *session->GetMap().GetId() + " already exists"s);
    } else {
        try {
            sessions_.emplace_back(session);
        } catch (...) {
            map_id_to_session_index_.erase(it);
            throw;
        }
    }
}

std::shared_ptr<GameSession> Game::FindSession(const Map::Id& id) const {
    if (auto it = map_id_to_session_index_.find(id); it != map_id_to_session_index_.end()) {
        return sessions_.at(it->second);
    }
    return nullptr;
}
std::vector<std::shared_ptr<GameSession>> Game::GetSessions() const {
    return sessions_;
}

const std::shared_ptr<GameSession> Game::ConnectToSession(Map::Id map_id, std::shared_ptr<Dog> dog, bool random) {
    auto map = FindMap(map_id);
    if(!map) {
        throw std::invalid_argument("Map with id "s + *map_id + " does not exist"s);
    }
    // Ищем подходящую сессию, если не нашли - создаем новую
    std::shared_ptr<GameSession> sessionPtr = FindSession(map_id);
    if(!sessionPtr) {
        sessionPtr = std::make_shared<GameSession>(GameSession::Id{SessionId::GetId()}, *map);
        AddSession(sessionPtr);
    }

    sessionPtr->AddDog(dog, random);
    return sessionPtr;
}

size_t Game::LootGenerate(std::chrono::milliseconds time_delta, unsigned loot_count, unsigned looter_count) {
    return loot_generator_->Generate(time_delta, loot_count, looter_count);
}

}  // namespace model
