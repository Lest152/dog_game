#pragma once

#include <memory>

#include "RetiredPlayers.h"

namespace app {

class UnitOfWork {
public:
    virtual void Commit() = 0;

    virtual void SaveRetiredPlayer(const model::RetiredPlayer& retired_player) = 0;
    virtual std::vector<model::RetiredPlayer> LoadRetiredPlayers(int start, int max_items) = 0;
protected:
    ~UnitOfWork() = default;
private:
    virtual std::shared_ptr<model::RetiredPlayerRepository> RetiredPlayer() = 0;
};

class UnitOfWorkFactory {
public:
    virtual std::shared_ptr<app::UnitOfWork> CreateUnitOfWork() = 0;
protected:
    ~UnitOfWorkFactory() = default;
};

}   // namespace app