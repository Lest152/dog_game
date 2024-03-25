#pragma once

#include <filesystem>
#include <boost/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <tuple>
#include <optional>

#include "model.h"

namespace json_loader {
using Roads = std::vector<std::tuple<int, int, int, int>>;

model::Game LoadGame(const std::filesystem::path& json_path);
boost::json::array GetArrayMaps(boost::json::value& json);
std::string GetMapId(boost::json::value& json);
std::string GetMapName(boost::json::value& json);

}  // namespace json_loader
