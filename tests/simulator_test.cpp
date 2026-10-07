#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "logging/trade_journal.hpp"
#include "simulator/simulator.hpp"
#include "simulator_test_data.hpp"

namespace {
    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    class EmptyInputSource final : public sim::IInputSource {
    public:
        bool read_chunk(const char *&, size_t &) override {
            return false;
        }
    };

    class RecordingStrategy final : public sim::IStrategy {
    public:
        void on_event(const sim::MarketEvent &event) override {
            events.push_back(event.stock_locate);
        }

        void on_trade(const sim::SimulatedTrade &trade) override {
            trades.push_back(trade);
        }

        std::vector<sim::SimulatedTrade> trades;
        std::vector<dt::StockLocate> events;
    };

    std::vector<logging::TradeJournalRecord> read_records(
        const std::filesystem::path &path) {
        std::ifstream file(path);
        require(file.good(), "journal file was not created");

        std::vector<logging::TradeJournalRecord> records;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                records.push_back(nlohmann::json::parse(line)
                                      .get<logging::TradeJournalRecord>());
            }
        }
        return records;
    }

    std::string read_file(const std::filesystem::path &path) {
        std::ifstream file(path);
        return {std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()};
    }

    bool same_trade(const sim::SimulatedTrade &lhs, const sim::SimulatedTrade &rhs) {
        return lhs.timestamp == rhs.timestamp &&
               lhs.side == rhs.side &&
               lhs.price == rhs.price &&
               lhs.quantity == rhs.quantity &&
               lhs.stock_locate == rhs.stock_locate;
    }

    bool wait_for_record_count(const std::filesystem::path &path,
                               std::size_t expected) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            if (read_records(path).size() == expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

    void test_on_stop_journal_is_simulator_source_of_truth() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_simulator_on_stop_journal_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        auto strategy = std::make_unique<RecordingStrategy>();
        auto *strategy_ptr = strategy.get();
        sim::Simulator simulator(
            std::make_unique<EmptyInputSource>(),
            std::move(strategy),
            {.trade_journal = {
                .path = path,
                .mode = logging::TradeJournal::Mode::OnStop}});

        const sim::SimulatedTrade first{10, sim::Side::Buy, 101, 5, 1};
        const sim::SimulatedTrade second{11, sim::Side::Sell, 100, 6, 2};
        simulator.save_trade(first);
        simulator.save_trade(second);

        require(!std::filesystem::exists(path),
                "on-stop journal must not write before stop");
        require(strategy_ptr->trades.size() == 2,
                "strategy must receive one callback per saved trade");
        require(same_trade(strategy_ptr->trades[0], first),
                "first strategy callback trade mismatch");
        require(same_trade(strategy_ptr->trades[1], second),
                "second strategy callback trade mismatch");

        const auto running_stats = simulator.journal_stats();
        require(running_stats.records_accepted == 2,
                "simulator did not expose running journal acceptance count");
        require(running_stats.records_written == 0,
                "on-stop simulator journal wrote before stop");

        simulator.stop();
        const auto stopped_stats = simulator.journal_stats();
        require(stopped_stats.records_written == 2,
                "simulator did not expose final journal write count");
        const auto records = read_records(path);
        require(records.size() == 2, "simulator journal record count mismatch");
        require(records[0].sequence == 0 && records[1].sequence == 1,
                "simulator journal sequence mismatch");
        require(same_trade(records[0].trade, first), "first journal trade mismatch");
        require(same_trade(records[1].trade, second), "second journal trade mismatch");

        const auto first_save = read_file(path);
        simulator.stop();
        require(read_file(path) == first_save, "repeated stop must be idempotent");

        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_real_time_journal_is_visible_before_stop() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_simulator_real_time_journal_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        sim::Simulator simulator(
            std::make_unique<EmptyInputSource>(),
            std::make_unique<RecordingStrategy>(),
            {.trade_journal = {
                .path = path,
                .mode = logging::TradeJournal::Mode::RealTime}});

        const sim::SimulatedTrade trade{20, sim::Side::Buy, 201, 15, 3};
        simulator.save_trade(trade);

        require(wait_for_record_count(path, 1),
                "real-time simulator journal was not visible before stop");
        const auto records = read_records(path);
        require(records.size() == 1, "real-time simulator journal record count mismatch");
        require(same_trade(records[0].trade, trade),
                "real-time simulator journal trade mismatch");

        simulator.stop();
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_empty_run_writes_empty_journal() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_simulator_empty_journal_test.jsonl";
        auto stats_path = path;
        stats_path += ".stats.json";
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);

        sim::Simulator simulator(
            std::make_unique<EmptyInputSource>(),
            std::make_unique<RecordingStrategy>(),
            {.trade_journal = {
                .path = path,
                .mode = logging::TradeJournal::Mode::OnStop}});
        simulator.run();

        require(read_records(path).empty(), "empty run must write an empty journal");
        std::filesystem::remove(path);
        std::filesystem::remove(stats_path);
    }

    void test_disabled_journal_does_not_create_file() {
        const auto path = std::filesystem::temp_directory_path() /
                          "hft_simulator_disabled_journal_test.jsonl";
        std::filesystem::remove(path);

        auto strategy = std::make_unique<RecordingStrategy>();
        auto *strategy_ptr = strategy.get();
        sim::Simulator simulator(
            std::make_unique<EmptyInputSource>(),
            std::move(strategy),
            {.trade_journal = {
                .path = path,
                .mode = logging::TradeJournal::Mode::Disabled}});

        simulator.save_trade(sim::SimulatedTrade{30, sim::Side::Sell, 301, 25, 4});
        simulator.stop();

        require(strategy_ptr->trades.size() == 1,
                "disabled journal must not disable strategy trade callbacks");
        require(!std::filesystem::exists(path),
                "disabled simulator journal must not create a file");
    }

    void test_persistence_failure_is_reported() {
        sim::Simulator simulator(
            std::make_unique<EmptyInputSource>(),
            std::make_unique<RecordingStrategy>(),
            {.trade_journal = {
                .path = std::filesystem::temp_directory_path(),
                .mode = logging::TradeJournal::Mode::OnStop}});

        bool threw = false;
        try {
            simulator.stop();
        } catch (const std::runtime_error &) {
            threw = true;
        }
        require(threw, "simulator must report journal persistence failure");
    }

    void test_default_simulator_preserves_legacy_export() {
        const auto unique_suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto directory = std::filesystem::temp_directory_path() /
                               ("hft_simulator_default_journal_" + unique_suffix);
        std::filesystem::create_directory(directory);

        const auto original_directory = std::filesystem::current_path();
        try {
            std::filesystem::current_path(directory);
            sim::Simulator simulator(
                std::make_unique<EmptyInputSource>(),
                std::make_unique<RecordingStrategy>());
            simulator.save_trade(sim::SimulatedTrade{40, sim::Side::Buy, 401, 35, 5});
            simulator.stop();
            const auto document = nlohmann::json::parse(read_file("trades.json"));
            const sim::SimulatedTrade expected{40, sim::Side::Buy, 401, 35, 5};
            require(document == nlohmann::json{{"trades", {expected}}},
                    "legacy filename or JSON schema changed");
            const auto saved = read_file("trades.json");
            simulator.stop();
            require(read_file("trades.json") == saved, "legacy stop is not idempotent");
            std::filesystem::remove("trades.json");
            sim::Simulator empty(std::make_unique<EmptyInputSource>(),
                                 std::make_unique<RecordingStrategy>());
            empty.run();
            require(!std::filesystem::exists("trades.json"),
                    "empty legacy run must not create trades.json");
            std::filesystem::current_path(original_directory);
        } catch (...) {
            std::filesystem::current_path(original_directory);
            std::filesystem::remove_all(directory);
            throw;
        }

        require(!std::filesystem::exists(directory / logging::TradeJournal::DEFAULT_PATH),
                "default simulator must not create a journal file");
        require(!std::filesystem::exists(
                    directory / (logging::TradeJournal::DEFAULT_PATH + ".stats.json")),
                "default simulator must not create a stats file");
        std::filesystem::remove_all(directory);
    }

    void test_stock_capacity_and_rejection_state() {
        auto strategy = std::make_unique<RecordingStrategy>();
        auto *recording = strategy.get();
        sim::Simulator::Config config{.event_throttle = 1};
        for (dt::StockLocate locate = 1; locate <= md::ITCHParser::MaxTrackedStocks; ++locate) {
            config.tracked_stocks.push_back(locate);
        }
        sim::Simulator simulator(std::make_unique<EmptyInputSource>(),
                                 std::move(strategy), config);
        simulator.add_tracked_stock(16); // Duplicate at capacity remains a no-op.
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            bool threw = false;
            try {
                simulator.add_tracked_stock(17);
            } catch (const std::invalid_argument &) {
                threw = true;
            }
            require(threw, "failed addition changed tracking state or accepted stock 17");
        }
        for (dt::StockLocate locate : {16, 17}) {
            auto data = simulator_test_data::add_order(locate);
            md::Cursor cursor(data.data(), data.size());
            const auto parsed = simulator.process_one(cursor);
            require(parsed.parsed == (locate == 16), "parser tracking diverged after rejection");
        }
        require(recording->events == std::vector<dt::StockLocate>{16},
                "market routing diverged after rejection");
        config.tracked_stocks.push_back(17);
        bool threw = false;
        try {
            sim::Simulator invalid(std::make_unique<EmptyInputSource>(),
                                   std::make_unique<RecordingStrategy>(), config);
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        require(threw, "C++ config accepted more than 16 stocks");
        config.tracked_stocks = {1, 1};
        threw = false;
        try {
            sim::Simulator invalid(std::make_unique<EmptyInputSource>(),
                                   std::make_unique<RecordingStrategy>(), config);
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        require(threw, "C++ config accepted duplicate stocks");
    }

    void test_run_metrics_use_chunked_input_and_finalize() {
        auto data = simulator_test_data::add_order(7);
        data.insert(data.end(), {0, 1, 'S'});
        for (const bool measured : {false, true}) {
            auto input = std::make_unique<simulator_test_data::ChunkedInput>(data, 1);
            auto *source = input.get();
            auto strategy = std::make_unique<RecordingStrategy>();
            auto *recording = strategy.get();
            sim::Simulator simulator(std::move(input), std::move(strategy),
                                     {.tracked_stocks = {7}, .event_throttle = 1});
            sim::Simulator::RunMetrics metrics;
            if (measured) simulator.run(metrics);
            else simulator.run();
            require(source->reads == data.size() + 1, "run bypassed chunked input");
            require(recording->events == std::vector<dt::StockLocate>{7},
                    "run did not route a split frame through the market");
            if (measured) {
                require(metrics.parsed == 1 && metrics.skipped == 1 && metrics.input_complete,
                        "run metrics failed to aggregate split frames");
                require(metrics.total_seconds >= metrics.processing_seconds +
                        metrics.finalization_seconds, "run total excludes a phase");
            }
            bool stopped = false;
            try {
                simulator.save_trade(sim::SimulatedTrade{});
            } catch (const std::logic_error &) {
                stopped = true;
            }
            require(stopped, "run did not finalize the simulator");
        }
        data.pop_back();
        sim::Simulator incomplete(
            std::make_unique<simulator_test_data::ChunkedInput>(data, 1),
            std::make_unique<RecordingStrategy>(), {});
        sim::Simulator::RunMetrics metrics;
        incomplete.run(metrics);
        require(!metrics.input_complete && metrics.parsed == 1,
                "run accepted a truncated final frame");
    }
}

int main() {
    test_on_stop_journal_is_simulator_source_of_truth();
    test_real_time_journal_is_visible_before_stop();
    test_empty_run_writes_empty_journal();
    test_disabled_journal_does_not_create_file();
    test_persistence_failure_is_reported();
    test_default_simulator_preserves_legacy_export();
    test_stock_capacity_and_rejection_state();
    test_run_metrics_use_chunked_input_and_finalize();
    return 0;
}
