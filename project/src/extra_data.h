#pragma once

#include <boost/json.hpp>
#include <vector>

namespace extra_data {

    class LootTypes {
    public:
        explicit LootTypes(boost::json::array types);

        size_t GetLootCount() const;
        size_t GetScore(size_t type) const;
    private:
        boost::json::array loot_types_;
        std::vector<size_t> scores_;
    };
} // namespace extra_data