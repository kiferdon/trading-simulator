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

class DebugStrategy : public sim::IStrategy {
public:
  enum class FlushMode { RealTime, OnFinish };

  struct Config {
    bool log_events = false;
    bool log_trades = false;
    FlushMode flush_mode = FlushMode::RealTime;
    std::string log_file_path;
  };

  explicit DebugStrategy(Config config) : config_(std::move(config)) {
    if (config_.log_events || config_.log_trades) {
      log_file_.open(config_.log_file_path, std::ios::app);
    }
  }

  ~DebugStrategy() override {
    if (config_.flush_mode == FlushMode::OnFinish && log_file_.is_open()) {
      for (const auto &line: log_buffer_) {
        log_file_ << line;
      }
      log_buffer_.clear();
    }
    log_file_.close();
  }

  void on_event(const sim::MarketEvent &event) override {
    constexpr uint32_t SPREAD_THRESHOLD_PCT = 20; // 20 = 2.0%

    if (event.snapshot.best_bid > 0 &&
        event.snapshot.best_ask > event.snapshot.best_bid) {
      uint32_t spread = event.snapshot.best_ask - event.snapshot.best_bid;
      uint64_t mid = (static_cast<uint64_t>(event.snapshot.best_bid) +
                      event.snapshot.best_ask) /
                     2;

      // Avoid overflow: compare as scaled integers (spread/ask and spread/bid)
      if (spread * 100 < static_cast<uint32_t>(mid) * SPREAD_THRESHOLD_PCT) {
        decide_to_trade(event, spread);
      }
    }

    if (!config_.log_events)
      return;

    log_file_ << "[EVENT] time=" << event.timestamp
        << " stock=" << event.stock_locate
        << " bid=" << event.snapshot.best_bid
        << " ask=" << event.snapshot.best_ask << "\n";
  }

  void on_trade(const sim::SimulatedTrade &trade) override {
    if (!config_.log_trades)
      return;

    log_file_ << "[TRADE] time=" << trade.timestamp
        << " side=" << (trade.side == sim::Side::Buy ? "BUY" : "SELL")
        << " price=" << trade.price << " qty=" << trade.quantity << "\n";
  }

  void set_simulator(sim::Simulator *sim) { simulator_ = sim; }

private:
  void decide_to_trade(const sim::MarketEvent &event, uint32_t spread) {
    if (!simulator_)
      return;

    sim::SimulatedTrade trade{};
    trade.timestamp = event.timestamp;
    trade.quantity = 100;
    trade.stock_locate = event.stock_locate;
    trade.side = buy_next_ ? sim::Side::Buy : sim::Side::Sell;
    buy_next_ = !buy_next_;
    trade.price = (trade.side == sim::Side::Buy)
                    ? event.snapshot.best_ask
                    : event.snapshot.best_bid;

    simulator_->save_trade(trade);
  }

  Config config_;
  std::ofstream log_file_;
  sim::Simulator *simulator_ = nullptr;
  std::vector<std::string> log_buffer_;
  bool buy_next_ = true;
};

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
                            ? DebugStrategy::FlushMode::OnFinish
                            : DebugStrategy::FlushMode::RealTime;

  try {
    auto input = std::make_unique<sim::FileInputSource>(
      path,
      chunk_size > 0
        ? sim::FileInputSource::Mode::Chunked
        : sim::FileInputSource::Mode::WholeFile,
      chunk_size, file_portion);

    DebugStrategy::Config config;
    config.log_events = log_events;
    config.log_trades = log_trades;
    config.flush_mode = flush_mode;
    config.log_file_path = "strategy.log";
    auto strategy = std::make_unique<DebugStrategy>(config);

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
