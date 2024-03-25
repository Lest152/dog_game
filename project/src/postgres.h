#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/pqxx>

#include <condition_variable>

#include "RetiredPlayers.h"
#include "UnitOfWork.h"

namespace postgres {

class ConnectionPool {
    using PoolType = ConnectionPool;
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

public:
    class ConnectionWrapper {
    public:
        ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
            : conn_{std::move(conn)}
            , pool_{&pool} {
        }

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }

        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }

    private:
        std::shared_ptr<pqxx::connection> conn_;
        PoolType* pool_;
    };

    // ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection() {
        std::unique_lock lock{mutex_};
        // Блокируем текущий поток и ждём, пока cond_var_ не получит уведомление и не освободится
        // хотя бы одно соединение
        cond_var_.wait(lock, [this] {
            return used_connections_ < pool_.size();
        });
        // После выхода из цикла ожидания мьютекс остаётся захваченным

        return {std::move(pool_[used_connections_++]), *this};
    }

private:
    void ReturnConnection(ConnectionPtr&& conn) {
        // Возвращаем соединение обратно в пул
        {
            std::lock_guard lock{mutex_};
            assert(used_connections_ != 0);
            pool_[--used_connections_] = std::move(conn);
        }
        // Уведомляем один из ожидающих потоков об изменении состояния пула
        cond_var_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

class RetiredPlayerRepositoryImpl : public model::RetiredPlayerRepository {
public:
    explicit RetiredPlayerRepositoryImpl(pqxx::work& work)
        : work_{work} {
    }

    void Save(const model::RetiredPlayer& retired_player) override;

    std::vector<model::RetiredPlayer> Load(int start, int max_items) override;

private:
    pqxx::work& work_;
};

class UnitOfWorkImpl : public app::UnitOfWork {
public:
    explicit UnitOfWorkImpl(ConnectionPool::ConnectionWrapper& conn_wrapper)
        : work_(*conn_wrapper) {
    }

    void Commit() override;
    void SaveRetiredPlayer(const model::RetiredPlayer& retired_player) override;

    std::vector<model::RetiredPlayer> LoadRetiredPlayers(int start, int max_items) override;
private:
    pqxx::work work_;
    std::shared_ptr<model::RetiredPlayerRepository> RetiredPlayer() override;
};

class UnitOfWorkFactoryImpl : public app::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl() {
    }

    std::shared_ptr<app::UnitOfWork> CreateUnitOfWork() override;

    void SetConnPull(std::shared_ptr<ConnectionPool> conn_pull) {
        conn_pool_ = conn_pull;
    }
    
private:
    std::shared_ptr<ConnectionPool> conn_pool_;
};

class Database {
public:
    explicit Database(const char* db_url);

    UnitOfWorkFactoryImpl& GetUnitOfWorkFactory() &;

private:
    std::shared_ptr<ConnectionPool> conn_pool_;
    UnitOfWorkFactoryImpl unit_of_work_factory_;
};

}   // namespace postgres