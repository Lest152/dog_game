#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <random>
#include <iostream>
#include <cmath>
#include <optional>
#include <tuple>

#include "tagged.h"
#include "loot_generator.h"
#include "extra_data.h"
#include "tagged_uuid.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Speed {
    double horizont = 0.0;
    double vertical = 0.0;
};
 
struct Coordinate {
    double x = 0.0;
    double y = 0.0;
};

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

bool operator<=(const Coordinate& coord, const Point& point);

bool operator>=(const Coordinate& coord, const Point& point);

class LostObjects {
public:
    using Object = std::tuple<size_t, Coordinate, size_t>;

    void AddObject(const Coordinate&, size_t);
    void DeleteObject(size_t);
    size_t GetCountObjects() const;
    std::vector<Object> GetObjects() const;
private:
    size_t id_ = 0;
    std::vector<Object> objects_;
};

class Bag {
public:
    using Object = std::pair<size_t, size_t>;

    void AddObject(size_t id, size_t type);
    void FreeBag();

    std::vector<Object> GetBag() const;
private:
    std::vector<Object> objects_;
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {}

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {}

    bool IsHorizontal() const noexcept;
    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;
    Point GetEnd() const noexcept;

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, std::string config, extra_data::LootTypes loot_types) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , config_(std::move(config))
        , loot_types_(std::move(loot_types)) {}

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    const Buildings& GetBuildings() const noexcept;

    const Roads& GetRoads() const noexcept;

    const Offices& GetOffices() const noexcept;

    const std::string& GetConfig() const noexcept;

    double GetDogSpeed() const;

    void AddRoad(const Road& road);

    void AddBuilding(const Building& building);

    void AddOffice(Office office);

    void SetDogSpeed(double speed);

    void SetBagCapacity(int);

    int GetBagCapacity() const;

    size_t GetLootTypesCount() const;

    size_t GetScoreLootType(size_t type) const;

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double dog_speed_ = 1.0;
    int bag_capacity_ = 3;
    Roads roads_;
    Buildings buildings_;
    std::string config_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    extra_data::LootTypes loot_types_;
};

class Dog {
public:
    using Id = util::Tagged<size_t, Dog>;

    Dog(Id id, std::string name)
        : id_{id}
        , name_(move(name)) {}

    Id GetId() const;
    std::string GetName() const;
    Coordinate GetPos() const;
    std::string GetDir() const;
    Speed GetSpeed() const;
    std::vector<Bag::Object> GetBagObjects() const;
    size_t GetScore() const;

    void SetPos(const Coordinate& pos);
    void SetDir(std::string_view dir, double speed);
    void SetSpeed(const Speed& speed);

    void AddItem(size_t id, size_t type, size_t score);
    void FreeItems();
    int GetItemsCount() const;

    void AddPlayTime(const double t);
    double GetStopTime() const;
    double GetPlayTime() const;

private:
    Id id_;
    std::string name_;
    Coordinate pos_;
    Speed speed_;
    std::string dir_ = "U";
    int items_count_ = 0;
    Bag bag_;
    size_t score_ = 0;
    double play_time_ = 0.0;
    double stop_time_ = 0.0;
    bool is_move_ = false;
};

// Создает уникальные id собак
class DogsId {
public:
    static size_t GetDogId();
private:
    inline static size_t id_ = 0;
};

// Создает уникальные id сессий
class SessionId {
public:
    static size_t GetId();
private:
    inline static size_t id_ = 0;
};

class GameSession {
public:
    using Id = util::Tagged<size_t, GameSession>;
    using Dogs = std::vector<std::shared_ptr<Dog>>;

    GameSession(Id id, Map map)
        : id_{id}
        , map_{map} {}
        
    Id GetId() const;
    std::shared_ptr<Dogs> GetDogs() const;
    size_t DogsCount() const;
    size_t LootCount() const;
    void AddDog(std::shared_ptr<Dog>, bool);
    void DeleteDog(std::shared_ptr<Dog>);
    void AddLoot();
    void DeliteLoot(size_t);
    std::vector<LostObjects::Object> GetLootObjects() const;
    const Map GetMap() const;
    Coordinate GetRandomCoordinate() const;

private:
    Id id_;
    Dogs dogs_;
    Map map_;
    LostObjects lost_objects_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    Game() {}
    explicit Game(std::shared_ptr<loot_gen::LootGenerator> loot_generator)
        : loot_generator_{loot_generator} {}

    void AddMap(Map map);
    const Maps& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;

    void AddSession(std::shared_ptr<GameSession> session);
    std::shared_ptr<GameSession> FindSession(const Map::Id& id) const;
    std::vector<std::shared_ptr<GameSession>> GetSessions() const;

    const std::shared_ptr<GameSession> ConnectToSession(Map::Id map_id, std::shared_ptr<Dog> dog, bool random);

    size_t LootGenerate(std::chrono::milliseconds time_delta, unsigned loot_count, unsigned looter_count);

    void SetDogRetirementTime(const float dog_retirement_time);

    const float GetDogRetirementTime() const;

private:
    using MapIdHasher  = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    std::vector<std::shared_ptr<GameSession>> sessions_;
    MapIdToIndex map_id_to_session_index_;

    std::shared_ptr<loot_gen::LootGenerator> loot_generator_;

    float dog_retirement_time_;
};

}  // namespace model
