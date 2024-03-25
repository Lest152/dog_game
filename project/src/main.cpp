#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <optional>

#include "json_loader.h"
#include "request_handler.h"
#include "logger_handler.h"

#include "ticker.h"

#include "postgres.h"
#include "UseCases.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace http = boost::beast::http;

struct Args {
    int period;
    std::string config;
    std::string static_path;

    bool is_period = false;
    bool is_random = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    Args args;
    desc.add_options()
        // Добавляем опцию --help и её короткую версию -h
        ("help,h", "produce help message")
        // Опция --tick-period (-t), задаёт период автоматического обновления игрового состояния в миллисекундах
        ("tick-period,t", po::value(&args.period)->value_name("milliseconds"s), "set tick period")
        // Опция --config-file (-c) задаёт путь к конфигурационному JSON-файлу игры
        ("config-file,c", po::value(&args.config)->value_name("file"s), "set config file path")
        // Опция --www-root (-w), задаёт путь к каталогу со статическими файлами игры
        ("www-root,w", po::value(&args.static_path)->value_name("dir"s), "set static files root")
        // Опция --randomize-spawn-points, включает режим, при котором пёс игрока появляется в случайной точке случайно выбранной дороги карты
        ("randomize-spawn-points", "spawn dogs at random positions ");
        

    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }

    // Проверяем наличие опций src и dst
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file not found"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("static path not found"s);
    }
    if (vm.contains("tick-period"s)) {
        args.is_period = true;
    }
    if (vm.contains("randomize-spawn-points"s)) {
        args.is_random= true;
    }


    // С опциями программы всё в порядке, возвращаем структуру args
    return args;
}

namespace {
// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned thread_cnt, const Fn& fn) {
    thread_cnt = std::max(1u, thread_cnt);
    std::vector<std::jthread> workers;
    workers.reserve(thread_cnt - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--thread_cnt) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    std::optional<Args> args;

    try {
        args = ParseCommandLine(argc, argv);
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    try {

        const char* db_url = std::getenv("GAME_DB_URL");
        if(!db_url) {
            throw std::runtime_error("DB URL is not specified");
        }

        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args->config);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if(!ec) {
                ioc.stop();
            }
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        std::string ip = "0.0.0.0";
        const auto address = net::ip::make_address(ip);
        constexpr net::ip::port_type port = 8080;      

        // strand для выполнения запросов к API
        auto api_strand = net::make_strand(ioc);

        // Инициализация БД
        postgres::Database db{db_url};
        app::UseCasesImpl use_cases{db.GetUnitOfWorkFactory()};

        // Токены и игроки
        std::shared_ptr<app::PlayerTokens> tokens  = std::make_shared<app::PlayerTokens>();
        std::shared_ptr<app::Players>      players = std::make_shared<app::Players>();
        // Объект Application содержит сценарии использования
        app::Application app {game, tokens, players, args->is_random, args->is_period, use_cases};

        // Настраиваем вызов метода Application::Tick каждые n миллисекунд внутри strand
        if(args->is_period) {
            std::chrono::milliseconds duration(args->period);
            auto ticker = std::make_shared<Ticker>(api_strand, duration,
                [&app](std::chrono::milliseconds delta) { app.Tick(delta); }
            );
            ticker->Start();
        }

        auto handler = std::make_shared<http_handler::RequestHandler>(app, args->static_path, api_strand);
        http_handler::LoggingRequestHandler<http_handler::RequestHandler> loging_handler{*handler, port, ip};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        http_server::ServeHttp(ioc, {address, port}, [&loging_handler](std::string&& ip, auto&& req, auto&& send) {
            loging_handler(std::forward<std::string>(ip), std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        //std::cout << "Server has started..."sv << std::endl;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
