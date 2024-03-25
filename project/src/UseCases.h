#pragma once
#include <string>
#include <vector>

#include "UnitOfWork.h"

namespace app {

namespace detail {
    struct RetiredPlayerInfo {
        std::string name;
        double score;
        double play_time;
    };
}

class UseCases {
public:
    virtual void AddRetiredPLayer(const std::string& name, const double score, const double play_time) = 0;
    virtual std::vector<detail::RetiredPlayerInfo> GetRetiredPlayer(const double start, const double max_items) = 0;
protected:
    ~UseCases() = default;
};

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(UnitOfWorkFactory& unit_of_work_factory)
        : unit_of_work_factory_{unit_of_work_factory} {
    }

    void AddRetiredPLayer(const std::string& name, const double score, const double play_time) override;
    
    std::vector<detail::RetiredPlayerInfo> GetRetiredPlayer(const double start, const double max_items) override;
private:
    UnitOfWorkFactory& unit_of_work_factory_;
};

}   // namespace app