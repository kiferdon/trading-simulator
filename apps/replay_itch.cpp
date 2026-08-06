//
// Created by silay on 5/26/26.
//

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "common/common.hpp"
#include "input/input_source.hpp"
#include "market_data/itch_parser.hpp"
#include "simulator/simulator.hpp"
#include "simulator/strategy.hpp"
#include "strategies/debug_strategy.hpp"

int main(int argc, char *argv[]) {
    if (argc > 7) {
        std::cerr << "Usage: " << argv[0]
                << " [itch_file] [chunk_size] [file_portion] [log_events:0|1] "
                "[log_trades:0|1] [flush_mode:0=real_time|1=on_finish]"
                << std::endl;
        return 1;
    }

    const std::string path =
            (argc > 1) ? argv[1] : "../market-data/12302019.NASDAQ_ITCH50";
    const size_t chunk_size = (argc > 2) ? std::stoul(argv[2]) : 0;
    const double file_portion = (argc > 3) ? std::stod(argv[3]) : 1.0;
    const bool log_events = (argc > 4) ? std::stoi(argv[4]) : 0;
    const bool log_trades = (argc > 5) ? std::stoi(argv[5]) : 0;
    const auto flush_mode = (argc > 6 && std::stoi(argv[6]) == 1)
                                ? sim::DebugStrategy::FlushMode::OnFinish
                                : sim::DebugStrategy::FlushMode::RealTime;

    try {
        auto input = std::make_unique<sim::FileInputSource>(
            path,
            chunk_size > 0
                ? sim::FileInputSource::Mode::Chunked
                : sim::FileInputSource::Mode::WholeFile,
            chunk_size, file_portion);

        sim::DebugStrategy::Config config;
        config.log_events = log_events;
        config.log_trades = log_trades;
        config.flush_mode = flush_mode;
        config.log_file_path = "strategy.log";
        auto strategy = std::make_unique<sim::DebugStrategy>(config);

        sim::Simulator sim(std::move(input), std::move(strategy));
        sim.set_strategy_simulator();

        sim.add_tracked_stock(md::STOCK_LOCATE_QQQ);
        sim.set_event_throttle(std::optional<dt::StockLocate>(md::STOCK_LOCATE_QQQ),
                               10);
        sim.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
