#pragma once

#include <boost/json.hpp>
#include <random>
#include <sstream>
#include <ios>
#include <chrono>
#include "tagged.h"
#include "model.h"
#include "collision_detector.h"
#include "geom.h"
#include "UseCases.h"



namespace app {

namespace detail {
    struct TokenTag {};
}   // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;
namespace json  = boost::json;

class Player {
public:
    explicit Player(std::shared_ptr<model::GameSession> session, std::shared_ptr<model::Dog> dog)
        : session_{session}
        , dog_{dog} {}

    model::Dog::Id GetId() const;
    std::shared_ptr<model::GameSession> GetSession() const;
    std::shared_ptr<model::Dog> GetDog() const;
private:
    std::shared_ptr<model::GameSession> session_;
    std::shared_ptr<model::Dog> dog_;
};

class PlayerTokens {
    public:
        std::shared_ptr<Player> FindPlayerByToken(Token);
        Token AddPlayer(std::shared_ptr<Player>);
        void DeletePlayer(std::shared_ptr<Player>);

    private:
        std::random_device random_device_;
        std::mt19937_64 generator1_{ [this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }() };
        std::mt19937_64 generator2_{ [this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }() };

        Token GenerateToken();

        using TokenHasher = util::TaggedHasher<Token>;
        using TokenToPlayer = std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher>;

        TokenToPlayer token_to_player_;
    };

class Players {
public:
    using Id = util::Tagged<std::string, Players>;

    std::shared_ptr<Player> Add(std::shared_ptr<model::Dog>, std::shared_ptr<model::GameSession>);
    std::shared_ptr<Player> DeletePlayer(std::shared_ptr<model::Dog>, std::shared_ptr<model::GameSession>);

private:
    using PlayerIdHasher   = util::TaggedHasher<Id>;
    using PlayerIdToPlayer = std::unordered_map<Id, std::shared_ptr<Player>, PlayerIdHasher>;

    PlayerIdToPlayer player_id_to_player_;
};

class VectorItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    VectorItemGathererProvider(std::vector<collision_detector::Item> items,
                               std::vector<collision_detector::Gatherer> gatherers)
        : items_(items)
        , gatherers_(gatherers) {
    }

    
    size_t ItemsCount() const override {
        return items_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

class Application {
public:
    Application(model::Game& game,
                std::shared_ptr<app::PlayerTokens> tokens,
                std::shared_ptr<app::Players> players,
                bool random,
                bool tick,
                UseCases& use_cases)
        : game_{game}
        , tokens_{tokens}
        , players_{players}
        , is_random_{random}
        , is_tick_{tick}
        , use_cases_{use_cases} {}
        
    json::object ConnectToGame(std::string user_name, std::string map_id);
    json::object GetPlayers(std::string token);
    json::object GetState(std::string token);
    json::array GetRecords(int start, int max_items);
    json::object Move(std::string token, std::string_view dist);
    void Tick(std::chrono::milliseconds delta);
    void Tick(const int delta) const;
    model::Game& GetGameObj();

    const bool IsTick() const;
private:
    model::Game& game_;
    std::shared_ptr<app::PlayerTokens> tokens_;
    std::shared_ptr<app::Players>      players_;
    bool is_random_;
    bool is_tick_;
    UseCases& use_cases_;

    void CalcNewPos(std::shared_ptr<model::Dog> dog, model::Map::Roads& roads, std::vector<collision_detector::Gatherer>& gatherers, const int delta) const;
    void CheckPlayerDisconnect(std::shared_ptr<model::Dog> dog, const int delta) const;
};

}   // namespace app