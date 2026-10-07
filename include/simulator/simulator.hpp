//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_SIMULATOR_HPP
#define HFT_SIMULATOR_SIMULATOR_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "input/input_source.hpp"
#include "logging/trade_journal.hpp"
#include "market_data/itch_parser.hpp"
#include "simulator/market.hpp"
#include "simulator/strategy.hpp"

namespace sim {
    class Simulator {
    public:
        struct Config {
            std::optional<unsigned> cpu_affinity = std::nullopt;
            std::vector<dt::StockLocate> tracked_stocks;
            std::optional<uint32_t> event_throttle = std::nullopt;
            logging::TradeJournal::Config trade_journal;
            std::filesystem::path source_path;
        };

        // Optional run-level diagnostics. Counters are accumulated per chunk;
        // clocks and journal snapshots are read only at phase boundaries.
        struct RunMetrics {
            std::size_t parsed = 0;
            std::size_t skipped = 0;
            bool input_complete = false;
            double processing_seconds = 0.0;
            double finalization_seconds = 0.0;
            double total_seconds = 0.0;
            logging::TradeJournal::Stats journal_at_processing_end;
        };

        // Compatibility constructor: buffers trades and exports trades.json on
        // stop(). Use the Config overload for explicit JSONL journal behavior.
        Simulator(std::unique_ptr<IInputSource> input,
                  std::unique_ptr<IStrategy> strategy);

        Simulator(std::unique_ptr<IInputSource> input,
                  std::unique_ptr<IStrategy> strategy,
                  Config config);

        ~Simulator();

        void run();
        void run(RunMetrics &metrics);

        md::ParseOneResult process_one(md::Cursor &cursor);

        void stop();

        void add_tracked_stock(dt::StockLocate locate);

        void set_event_throttle(std::optional<dt::StockLocate> locate,
                                uint32_t every_n_events);

        void set_strategy_simulator();

        void save_trade(const SimulatedTrade &trade);

        [[nodiscard]] logging::TradeJournal::Stats journal_stats() const noexcept;

        [[nodiscard]] const Config &config() const noexcept;

        // Called by Market via event callback
        void on_market_event(const sim::MarketEvent &event);

    private:
        static Config prepare_config(Config config);

        void run_impl(RunMetrics *metrics);

        Config config_;
        std::unique_ptr<IInputSource> input_;
        std::unique_ptr<IStrategy> strategy_;
        md::ITCHParser parser_;
        std::unique_ptr<Market> market_;
        logging::TradeJournal trade_journal_;
        std::optional<std::vector<SimulatedTrade>> legacy_trades_;
        std::unordered_set<dt::StockLocate> tracked_stocks_;
        bool running_ = false;
        bool stopped_ = false;
    };

    Simulator::Config load_simulator_config(const std::filesystem::path &path);

    std::string_view trade_journal_mode_name(logging::TradeJournal::Mode mode);

    void to_json(nlohmann::json &json, const Simulator::Config &config);
} // namespace sim

#endif // HFT_SIMULATOR_SIMULATOR_HPP
