#include "UseCases.h"

namespace app {

void UseCasesImpl::AddRetiredPLayer(const std::string& name, const double score, const double play_time) {
    auto unit_of_work = unit_of_work_factory_.CreateUnitOfWork();
    model::RetiredPlayer retired_player{model::RetiredPlayerId::New(), name, score, play_time};
    unit_of_work->SaveRetiredPlayer(retired_player);
    unit_of_work->Commit();
}

std::vector<detail::RetiredPlayerInfo> UseCasesImpl::GetRetiredPlayer(const double start, const double max_items) {
    auto unit_of_work = unit_of_work_factory_.CreateUnitOfWork();
    auto retired_players = unit_of_work->LoadRetiredPlayers(start, max_items);
    std::vector<detail::RetiredPlayerInfo> result;
    for(const auto& retired_player : retired_players) {
        detail::RetiredPlayerInfo info{retired_player.GetName(), retired_player.GetScore(), retired_player.GetPlayTime()};
        result.emplace_back(info);
    }
    return result;
}

}   // namespace app