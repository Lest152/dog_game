#include "postgres.h"
#include <iostream>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

std::shared_ptr<app::UnitOfWork> UnitOfWorkFactoryImpl::CreateUnitOfWork() {
    ConnectionPool::ConnectionWrapper connection_wrapper = conn_pool_->GetConnection();
    std::shared_ptr<UnitOfWorkImpl> unit_of_work = std::make_shared<UnitOfWorkImpl>(connection_wrapper);
    return unit_of_work;
}

Database::Database(const char* db_url) {
    conn_pool_ = std::make_shared<ConnectionPool>(std::thread::hardware_concurrency(), [db_url] {
                                                        auto conn = std::make_shared<pqxx::connection>(db_url);
                                                        conn->prepare("select_one", "SELECT 1;");
                                                        return conn;
                                                    });
              
    auto conn = conn_pool_->GetConnection();
    pqxx::work work{*conn};
    
    work.exec(R"(CREATE TABLE IF NOT EXISTS retired_players (
                    id UUID CONSTRAINT retired_players_constraint PRIMARY KEY,
                    name varchar(100) NOT NULL,
                    score DOUBLE PRECISION NOT NULL,
                    play_time DOUBLE PRECISION NOT NULL
    );)"_zv);

    work.commit();

    unit_of_work_factory_.SetConnPull(conn_pool_);
}

UnitOfWorkFactoryImpl& Database::GetUnitOfWorkFactory() & {
    return unit_of_work_factory_;
}

void UnitOfWorkImpl::Commit() {
    work_.commit();
}

std::shared_ptr<model::RetiredPlayerRepository> UnitOfWorkImpl::RetiredPlayer() {
    return std::make_shared<RetiredPlayerRepositoryImpl>(work_);
}

void UnitOfWorkImpl::SaveRetiredPlayer(const model::RetiredPlayer& retired_player) {
    RetiredPlayer()->Save(retired_player);
}

std::vector<model::RetiredPlayer> UnitOfWorkImpl::LoadRetiredPlayers(int start, int max_items) {
    return RetiredPlayer()->Load(start, max_items);
}

void RetiredPlayerRepositoryImpl::Save(const model::RetiredPlayer& retired_player) {
    work_.exec_params(R"(INSERT INTO retired_players (id, name, score, play_time) VALUES ($1, $2, $3, $4))"_zv,
                        retired_player.GetId().ToString(), retired_player.GetName(), retired_player.GetScore(), retired_player.GetPlayTime());
}

std::vector<model::RetiredPlayer> RetiredPlayerRepositoryImpl::Load(int start, int max_items) {
    std::vector<model::RetiredPlayer> retired_players;
    pqxx::result res = work_.exec_params("SELECT id, name, score, play_time FROM retired_players ORDER BY score DESC, play_time OFFSET $1 LIMIT $2", start, max_items);
    
    for (const auto& row : res) {
        model::RetiredPlayer retired_player{model::RetiredPlayerId::FromString(row["id"].as<std::string>()),
                                            row["name"].as<std::string>(),
                                            row["score"].as<double>(),
                                            row["play_time"].as<double>()};

        retired_players.emplace_back(retired_player);
    }

    return retired_players;
}


}   // namespace postgres