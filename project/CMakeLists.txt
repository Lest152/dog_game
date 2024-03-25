cmake_minimum_required(VERSION 3.11)
project(game_server_lib CXX)  # Изменяем имя проекта
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Создаем статическую библиотеку
add_library(game_server_lib STATIC
  src/http_server.cpp
  src/http_server.h
  src/sdk.h
  src/model.h
  src/model.cpp
  src/ticker.h
  src/tagged.h
  src/tagged_uuid.h
  src/tagged_uuid.cpp
  src/boost_json.cpp
  src/json_loader.h
  src/json_loader.cpp
  src/request_handler.cpp
  src/request_handler.h
  src/logger_handler.h
  src/api_request_handler.cpp
  src/api_request_handler.h
  src/Players.cpp
  src/Players.h
  src/RetiredPlayers.h
  src/UnitOfWork.h
  src/UseCases.h
  src/UseCases.cpp
  src/loot_generator.cpp
  src/loot_generator.h
  src/extra_data.cpp
  src/extra_data.h
  src/collision_detector.cpp
  src/collision_detector.h
  src/extra_data.cpp
  src/extra_data.h
  src/geom.h
  src/postgres.cpp
  src/postgres.h
)

target_include_directories(game_server_lib PUBLIC CONAN_PKG::boost)
target_link_libraries(game_server_lib PUBLIC CONAN_PKG::boost Threads::Threads CONAN_PKG::libpqxx)

# Создаем исполняемый файл для сервера
add_executable(game_server
  src/main.cpp
)
target_link_libraries(game_server game_server_lib)

# Создаем исполняемый файл для тестов loot_generator
add_executable(loot_generator_tests
  tests/loot_generator_tests.cpp
)
target_include_directories(loot_generator_tests PRIVATE  CONAN_PKG::boost)
target_link_libraries(loot_generator_tests PRIVATE CONAN_PKG::catch2 game_server_lib Threads::Threads)

# Создаем исполняемый файл для тестов collision-detector
add_executable(collision_detector_tests
  tests/collision-detector-tests.cpp
)
target_include_directories(collision_detector_tests PRIVATE  CONAN_PKG::boost)
target_link_libraries(collision_detector_tests PRIVATE CONAN_PKG::catch2 game_server_lib Threads::Threads)
