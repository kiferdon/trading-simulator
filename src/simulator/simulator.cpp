//
// Created by silay on 5/26/26.
//

#include "simulator/simulator.hpp"
#include <nlohmann/json.hpp>

namespace sim {
  Simulator::Simulator(std::unique_ptr<IInputSource> input,
                       std::unique_ptr<IStrategy> strategy)
    : input_(std::move(input)), strategy_(std::move(strategy)) {
    market_ = std::make_unique<Market>();
    market_->set_event_handler(this);
  }

  Simulator::~Simulator() = default;

  void Simulator::run() {
    running_ = true;

    const char *data = nullptr;
    size_t size = 0;

    while (running_ && input_->read_chunk(data, size)) {
      process_data(data, size);
    }

    stop();
  }

  md::ParseOneResult Simulator::process_one(md::Cursor &cursor) {
    return parser_.parse_one(cursor, *market_);
  }

  void Simulator::stop() {
    if (!trades_.empty()) {
      trade_file_.open("trades.json");
      if (trade_file_.is_open()) {
        nlohmann::json j = {{"trades", trades_}};
        trade_file_ << j.dump(2);
        trade_file_.close();
      }
    }
    running_ = false;
  }

  void Simulator::process_data(const char *data, size_t size) {
    parser_.parse(data, size, *market_);
  }

  void Simulator::add_tracked_stock(dt::StockLocate locate) {
    if (tracked_stocks_.insert(locate).second) {
      parser_.set_track_all(false);
      parser_.add_tracked_stock(locate);
      market_->add_tracked_stock(locate);
    }
  }

  void Simulator::set_event_throttle(std::optional<dt::StockLocate> locate,
                                     uint32_t every_n_events) {
    market_->set_event_throttle(locate, every_n_events);
  }

  void Simulator::set_strategy_simulator() {
    if (strategy_) {
      strategy_->set_simulator(this);
    }
  }

  void Simulator::save_trade(const SimulatedTrade &trade) {
    trades_.push_back(trade);
    if (strategy_) {
      strategy_->on_trade(trade);
    }
  }

  void Simulator::on_market_event(const sim::MarketEvent &event) {
    if (strategy_) {
      strategy_->on_event(event);
    }
  }
} // namespace sim
