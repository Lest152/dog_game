#include "json_loader.h"

namespace json_loader {
    using namespace std::filesystem;
    using namespace std::literals;


static const float DEFAULT_DOG_SPEED = 1.0;
static const float DEFAULT_DOG_RETIREMENT_TIME = 60.0;
static const int   DEFAULT_BAG_CAPACITY = 3;
    
boost::json::array GetArrayMaps(boost::json::value& json) {
    return json.at("maps").get_array();
}

boost::json::array GetArrayRoads(boost::json::value& json) {
    return json.at("roads").get_array();
}

boost::json::array GetArrayBuildings(boost::json::value& json) {
    return json.at("buildings").get_array();
}

boost::json::array GetArrayOffices(boost::json::value& json) {
    return json.at("offices").get_array();
}

boost::json::array GetArrayLootTypes(boost::json::value& json) {
    return json.at("lootTypes").get_array();
}

int GetX0(boost::json::value& json) {
    return json.at("x0").as_int64();
}

int GetY0(boost::json::value& json) {
    return json.at("y0").as_int64();
}

float GetDefaultDogSpeed(boost::json::value& json) {
    if(json.as_object().contains("defaultDogSpeed")) {
        return json.at("defaultDogSpeed").as_double();
    } 

    return DEFAULT_DOG_SPEED;
}

float GetDogRetirementTime(boost::json::value& json) {
    if(json.as_object().contains("dogRetirementTime")) {
        return json.at("dogRetirementTime").as_double();
    } 

    return DEFAULT_DOG_RETIREMENT_TIME;
}

int GetDefaultBagCapacity(boost::json::value& json) {
    if(json.as_object().contains("defaultBagCapacity")) {
        return json.at("defaultBagCapacity").as_int64();
    }

    return DEFAULT_BAG_CAPACITY;
}

float GetLootGeneratorPeriod(boost::json::value& json) {
    return json.at("lootGeneratorConfig").at("period").as_double();
}

float GetLootGeneratorProbability(boost::json::value& json) {
    return json.at("lootGeneratorConfig").at("probability").as_double();
}

std::string GetOfficeId(boost::json::value& json) {
    std::string_view id = json.at("id").as_string();
    return std::string{id};
}

int GetOfficeX(boost::json::value& json) {
    return json.at("x").as_int64();
}

int GetOfficeY(boost::json::value& json) {
    return json.at("y").as_int64();
}

int GetOfficeOffsetX(boost::json::value& json) {
    return json.at("offsetX").as_int64();
}

int GetOfficeOffsetY(boost::json::value& json) {
    return json.at("offsetY").as_int64();
}

std::shared_ptr<int> GetX1(boost::json::value& json) {
    if(json.as_object().contains("x1")) {
        return std::make_shared<int>(json.at("x1").as_int64());
    }
    
    return nullptr;
}

std::shared_ptr<int> GetY1(boost::json::value& json) {
    if(json.as_object().contains("y1")) {
        return std::make_shared<int>(json.at("y1").as_int64());
    }
    
    return nullptr;
}

std::string GetMapId(boost::json::value& json) {
    std::string_view id = json.as_object().at("id").as_string();
    return std::string{id};
}

std::string GetMapName(boost::json::value& json) {
    std::string_view name = json.as_object().at("name").as_string();
    return std::string{name};
}

// std::optional используется, потому-что, ключ "dogSpeed" может быть не указан в конфиге
// и берется значение по умолчанию
std::optional<float> GetDogSpeed(boost::json::value& json) {
    if(json.as_object().contains("dogSpeed")) {
        std::optional<float> speed = json.at("dogSpeed").as_double();
        return speed;
    }

    return std::nullopt;
}

std::optional<int> GetBagCapacity(boost::json::value& json) {
    if(json.as_object().contains("bagCapacity")) {
        std::optional<int> capacity = json.at("bagCapacity").as_int64();
        return capacity;
    }

    return std::nullopt;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла
    std::filesystem::path path = json_path;

    if(!exists(path)) {
        path = current_path() / "data" / json_path;
    } 
    if(!exists(path)) {
        throw std::logic_error("config path not exists");
    }

    std::ifstream file(path);
    // Читаем содержимое файла в строку
    std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // Парсим JSON
    boost::json::value json = boost::json::parse(jsonStr);

    float dogRetirementTime  = GetDogRetirementTime(json);
    float default_speed      = GetDefaultDogSpeed(json);
    float loot_period        = GetLootGeneratorPeriod(json);
    float loot_probability   = GetLootGeneratorProbability(json);
    int default_bag_capacity = GetDefaultBagCapacity(json);

    int millisecondsValue = static_cast<int>(loot_period * 1000); // Конвертация float в миллисекунды
    std::chrono::milliseconds duration(millisecondsValue);
    std::shared_ptr<loot_gen::LootGenerator> loot_generator = std::make_shared<loot_gen::LootGenerator>(duration, loot_probability);
    model::Game game{loot_generator};
    game.SetDogRetirementTime(dogRetirementTime);
    // Обходим список карт
    const auto arr = GetArrayMaps(json);
    for(auto it = arr.begin(); it < arr.end(); it++) {
        auto value = *it;
        std::string id   = GetMapId(value);
        std::string name = GetMapName(value);
        auto roads       = GetArrayRoads(value);
        auto buildings   = GetArrayBuildings(value);
        auto offices     = GetArrayOffices(value);
        auto loot_types  = GetArrayLootTypes(value);

        boost::json::object json_config;
        json_config["id"]        = id;
        json_config["name"]      = name;
        json_config["roads"]     = roads;
        json_config["buildings"] = buildings;
        json_config["offices"]   = offices;
        json_config["lootTypes"] = loot_types;

        model::Map map{model::Map::Id{id}, name, boost::json::serialize(json_config), extra_data::LootTypes{loot_types}};
        // Устанавливаем скорость собаки на карте
        {
            std::optional<float> speed = GetDogSpeed(value);
            if(speed) {
                map.SetDogSpeed(*speed);
            } else {
                map.SetDogSpeed(default_speed);
            }
        }
        // Устанавливаем вместимость рюкзака на карте
        {
            std::optional<float> bag_capacity = GetBagCapacity(value);
            if(bag_capacity) {
                map.SetBagCapacity(*bag_capacity);
            } else {
                map.SetBagCapacity(default_bag_capacity);
            }
        }

        // Добавляем дороги на карте
        for(auto coord : roads) {
            model::Point p;
            p.x = GetX0(coord);
            p.y = GetY0(coord);

            std::shared_ptr<int> x1 = GetX1(coord);
            std::shared_ptr<int> y1 = GetY1(coord);

            std::shared_ptr<model::Road> roadPtr;

            if(x1) {
                roadPtr = std::make_shared<model::Road>(model::Road::HORIZONTAL, p, *x1);
            } else {
                roadPtr = std::make_shared<model::Road>(model::Road::VERTICAL, p, *y1);
            }

            map.AddRoad(*roadPtr);
        }      

        // Добавляем офисы на карте
        for(auto coord : offices) {
            std::string office_id = GetOfficeId(coord);
            int x = GetOfficeX(coord);
            int y = GetOfficeY(coord);
            int offsetX = GetOfficeOffsetX(coord);
            int offsetY = GetOfficeOffsetY(coord);

            model::Office::Id tag_office_id{office_id};          
            model::Point point{x, y};
            model::Offset offset{offsetX, offsetY};

            model::Office office{tag_office_id, point, offset};

            map.AddOffice(office);
        }   
        
        game.AddMap(map);
    }

    return game;
}

}  // namespace json_loader
