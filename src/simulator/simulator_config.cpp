#include "simulator/simulator.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
    void require_exact_keys(const nlohmann::json &object,
                            std::initializer_list<std::string_view> keys,
                            std::string_view location) {
        if (!object.is_object()) {
            throw std::invalid_argument(std::string(location) +
                                        " must be a JSON object");
        }
        if (object.size() != keys.size()) {
            throw std::invalid_argument(std::string(location) +
                                        " contains missing or unknown fields");
        }
        for (const auto key: keys) {
            if (!object.contains(key)) {
                throw std::invalid_argument(std::string(location) +
                                            " is missing field '" +
                                            std::string(key) + "'");
            }
        }
    }

    template<typename Integer>
    Integer read_unsigned(const nlohmann::json &value,
                          std::string_view location) {
        if (!value.is_number_unsigned()) {
            throw std::invalid_argument(std::string(location) +
                                        " must be a non-negative integer");
        }
        const auto parsed = value.get<uint64_t>();
        if (parsed > std::numeric_limits<Integer>::max()) {
            throw std::invalid_argument(std::string(location) +
                                        " is out of range");
        }
        return static_cast<Integer>(parsed);
    }

    std::optional<unsigned> read_optional_cpu(const nlohmann::json &value,
                                              std::string_view location) {
        if (value.is_null()) return std::nullopt;
        return read_unsigned<unsigned>(value, location);
    }

    logging::TradeJournal::Mode parse_mode(const nlohmann::json &value) {
        if (!value.is_string()) {
            throw std::invalid_argument("trade_journal.mode must be a string");
        }
        const auto mode = value.get<std::string>();
        using Mode = logging::TradeJournal::Mode;
        if (mode == "disabled") return Mode::Disabled;
        if (mode == "on_stop") return Mode::OnStop;
        if (mode == "real_time") return Mode::RealTime;
        if (mode == "real_time_separate_thread") {
            return Mode::RealTimeSeparateThread;
        }
        throw std::invalid_argument("unknown trade journal mode: " + mode);
    }
}

namespace sim {
    Simulator::Config load_simulator_config(const std::filesystem::path &path) {
        const auto resolved_path = std::filesystem::canonical(path);
        std::ifstream file(resolved_path);
        if (!file) {
            throw std::runtime_error("failed to open simulator config: " +
                                     resolved_path.string());
        }

        try {
            const auto json = nlohmann::json::parse(file);
            require_exact_keys(json,
                               {"cpu_affinity", "tracked_stocks",
                                "event_throttle", "trade_journal"},
                               "simulator config");
            const auto &journal = json.at("trade_journal");
            require_exact_keys(journal,
                               {"path", "mode", "logger_cpu",
                                "idle_spin_count"},
                               "trade_journal");

            Simulator::Config config;
            config.cpu_affinity = read_optional_cpu(
                json.at("cpu_affinity"), "simulator config.cpu_affinity");

            const auto &tracked = json.at("tracked_stocks");
            if (!tracked.is_array()) {
                throw std::invalid_argument(
                    "simulator config.tracked_stocks must be an array");
            }
            if (tracked.size() > md::ITCHParser::MaxTrackedStocks) {
                throw std::invalid_argument("tracked stocks exceed parser capacity");
            }
            std::unordered_set<dt::StockLocate> unique;
            for (const auto &entry: tracked) {
                const auto locate = read_unsigned<dt::StockLocate>(
                    entry, "simulator config.tracked_stocks[]");
                if (!unique.insert(locate).second) {
                    throw std::invalid_argument(
                        "simulator config.tracked_stocks contains duplicates");
                }
                config.tracked_stocks.push_back(locate);
            }

            if (json.at("event_throttle").is_null()) {
                config.event_throttle = std::nullopt;
            } else {
                const auto throttle = read_unsigned<uint32_t>(
                    json.at("event_throttle"),
                    "simulator config.event_throttle");
                if (throttle == 0) {
                    throw std::invalid_argument(
                        "simulator config.event_throttle must be positive");
                }
                config.event_throttle = throttle;
            }

            const auto path_value = journal.at("path");
            if (!path_value.is_string() ||
                path_value.get_ref<const std::string &>().empty()) {
                throw std::invalid_argument(
                    "trade_journal.path must be a non-empty string");
            }
            auto journal_path = std::filesystem::path(
                path_value.get<std::string>());
            if (journal_path.is_relative()) {
                journal_path = resolved_path.parent_path() / journal_path;
            }
            config.trade_journal = {
                .path = journal_path.lexically_normal(),
                .mode = parse_mode(journal.at("mode")),
                .idle_spin_count = read_unsigned<std::size_t>(
                    journal.at("idle_spin_count"),
                    "trade_journal.idle_spin_count"),
                .logger_cpu = read_optional_cpu(
                    journal.at("logger_cpu"), "trade_journal.logger_cpu")
            };
            config.source_path = resolved_path;

            if (config.trade_journal.mode ==
                    logging::TradeJournal::Mode::RealTimeSeparateThread &&
                config.cpu_affinity && config.trade_journal.logger_cpu &&
                *config.cpu_affinity == *config.trade_journal.logger_cpu) {
                throw std::invalid_argument(
                    "cpu_affinity and trade_journal.logger_cpu must differ in "
                    "real_time_separate_thread mode");
            }
            return config;
        } catch (const nlohmann::json::exception &error) {
            throw std::invalid_argument("invalid simulator config '" +
                                        resolved_path.string() + "': " +
                                        error.what());
        }
    }

    std::string_view trade_journal_mode_name(
        logging::TradeJournal::Mode mode) {
        using Mode = logging::TradeJournal::Mode;
        switch (mode) {
            case Mode::Disabled: return "disabled";
            case Mode::OnStop: return "on_stop";
            case Mode::RealTime: return "real_time";
            case Mode::RealTimeSeparateThread:
                return "real_time_separate_thread";
        }
        return "unknown";
    }

    void to_json(nlohmann::json &json, const Simulator::Config &config) {
        json = {
            {"cpu_affinity", config.cpu_affinity},
            {"tracked_stocks", config.tracked_stocks},
            {"event_throttle", config.event_throttle},
            {"trade_journal", {
                {"path", config.trade_journal.path.string()},
                {"mode", trade_journal_mode_name(config.trade_journal.mode)},
                {"logger_cpu", config.trade_journal.logger_cpu},
                {"idle_spin_count", config.trade_journal.idle_spin_count}
            }}
        };
    }
} // namespace sim
