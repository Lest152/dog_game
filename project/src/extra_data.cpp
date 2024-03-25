#include "extra_data.h"

namespace extra_data {

    LootTypes::LootTypes(boost::json::array types) : loot_types_(std::move(types)) {
        for(auto loot_type : loot_types_) {
            int value = loot_type.at("value").as_int64();
            scores_.emplace_back(value);
        }
    }

    size_t LootTypes::GetLootCount() const {
        return loot_types_.size();
    }

    size_t LootTypes::GetScore(size_t type) const {
        return scores_[type];
    }
} // namespace extra_data