#pragma once

#include "tagged_uuid.h"

namespace model {
namespace detail {
    struct RetiredPlayerTag {};
}

using RetiredPlayerId = util::TaggedUUID<detail::RetiredPlayerTag>;

class RetiredPlayer {
public:

    RetiredPlayer(RetiredPlayerId id, std::string name, double score, double play_time)
        : id_(std::move(id))
        , name_(std::move(name))
        , score_{score}
        , play_time_{play_time} {
    }

    const RetiredPlayerId& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const double GetScore() const noexcept {
        return score_;
    }

    const double GetPlayTime() const noexcept {
        return play_time_;
    }

private:
    RetiredPlayerId id_;
    std::string name_;
    double score_;
    double play_time_;
};

class RetiredPlayerRepository {
public:
    virtual void Save(const RetiredPlayer& retired_player) = 0;

    virtual std::vector<RetiredPlayer> Load(int start, int max_items) = 0;
protected:
    ~RetiredPlayerRepository() = default;
};

}   // namespace model