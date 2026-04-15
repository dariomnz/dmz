#pragma once

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <unordered_map>

#include "Utils.hpp"

namespace DMZ {
enum class StatType : int {
    Parse,
    Semantic,
    Semantic_Declarations,
    Semantic_Body,
    Semantic_Fill_deps,
    Semantic_Mark_deps,
    CFG,
    Codegen,
    Compile,
    Run,
    Total,
    size,
};
static std::unordered_map<StatType, std::string> StatType_to_str = {
    {StatType::Parse, "Parse"},
    {StatType::Semantic, "Semantic"},
    {StatType::Semantic_Declarations, "Declarations"},
    {StatType::Semantic_Body, "Body"},
    {StatType::Semantic_Fill_deps, "Fill deps"},
    {StatType::Semantic_Mark_deps, "Mark deps"},
    {StatType::CFG, "CFG"},
    {StatType::Codegen, "Codegen"},
    {StatType::Compile, "Compile"},
    {StatType::Run, "Run"},
    {StatType::Total, "Total"},
};
class Stats {
   public:
    struct NamedStat {
        std::string name;
        double time;
        void dump(size_t level, double parentTime) const {
            double percentage = time / parentTime * 100;
            std::cerr << indent_line(level, 0, true) << std::left << std::setw(20) << name;

            std::cerr << std::fixed << std::setprecision(2) << indent(2) << std::setw(5) << percentage << "%";
            std::cerr << std::fixed << std::setprecision(4) << indent(2) << std::setw(10) << time << "ms";

            std::cerr << "\n";
        }
    };
    struct Stat {
        StatType type;
        std::vector<Stat> subStats = {};
        std::vector<NamedStat> subNamedStats = {};

        void dump(size_t level, double parentTime) const {
            auto stats = Stats::instance();
            double time = stats.get_time(type);
            double percentage = time / parentTime * 100;
            std::cerr << indent_line(level, 0, true) << std::left << std::setw(20) << StatType_to_str[type];

            std::cerr << std::fixed << std::setprecision(2) << indent(2) << std::setw(5) << percentage << "%";
            std::cerr << std::fixed << std::setprecision(4) << indent(2) << std::setw(10) << time << "ms";
            if (type == StatType::Parse) {
                std::cerr << " " << stats.parsed_lines << " lines";
                double lines_per_second = (stats.parsed_lines * 1000.0) / time;
                std::cerr << std::fixed << std::setprecision(0) << " (" << lines_per_second << " lines/s)";
            }
            std::cerr << "\n";
            for (auto&& v : subNamedStats) {
                v.dump(level + 1, time);
            }
            for (auto&& v : subStats) {
                v.dump(level + 1, time);
            }
        }
    };

    uint64_t parsed_lines = 0;
    std::vector<Stat> stat_map = {
        Stat{.type = StatType::Parse},
        Stat{.type = StatType::Semantic,
             .subStats =
                 {
                     Stat{.type = StatType::Semantic_Declarations},
                     Stat{.type = StatType::Semantic_Body},
                     Stat{.type = StatType::Semantic_Fill_deps},
                     Stat{.type = StatType::Semantic_Mark_deps},
                 }},
        Stat{.type = StatType::CFG},
        Stat{.type = StatType::Codegen},
        Stat{.type = StatType::Compile},
        Stat{.type = StatType::Run},
    };
    std::array<double, static_cast<size_t>(StatType::size)> stat_array = {};

    Stat* find_stat(StatType t, std::vector<Stat>& nodes) {
        for (auto& node : nodes) {
            if (node.type == t) return &node;
            if (!node.subStats.empty()) {
                if (auto* found = find_stat(t, node.subStats)) return found;
            }
        }
        return nullptr;
    }

   public:
    void dump() {
        for (auto&& v : stat_map) {
            v.dump(0, get_time(StatType::Total));
        }
    }

    void add_time(StatType t, double time) { stat_array[static_cast<size_t>(t)] += time; }

    void add_time(StatType t, std::string name, double time) {
        Stat* target = find_stat(t, stat_map);
        if (target) {
            for (auto& ns : target->subNamedStats) {
                if (ns.name == name) {
                    ns.time += time;
                    return;
                }
            }
            target->subNamedStats.push_back(NamedStat{name, time});
        }
    }

    double get_time(StatType t) { return stat_array[static_cast<size_t>(t)]; }

    void add_parsed_line() { parsed_lines++; }

    bool enabled = false;

    static Stats& instance() {
        static Stats s;
        return s;
    }
};
#define __line2_ScopedTimer(type, name, line)          \
    ptr<__ScopedTimer> st##line;                       \
    if (Stats::instance().enabled) {                   \
        st##line = makePtr<__ScopedTimer>(type, name); \
    }
#define __line1_ScopedTimer(type, name, line) __line2_ScopedTimer(type, name, line)
#define ScopedTimer(type)                     __line1_ScopedTimer(type, "", __LINE__)
#define ScopedTimerName(type, name)           __line1_ScopedTimer(type, name, __LINE__)

class __ScopedTimer {
   private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    std::string name = "";
    StatType type;

   public:
    __ScopedTimer(StatType t, std::string name = "") : name(name), type(t) {
        start = std::chrono::high_resolution_clock::now();
    }
    ~__ScopedTimer() {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> to_add = now - start;
        if (name.empty()) {
            Stats::instance().add_time(type, to_add.count());
        } else {
            Stats::instance().add_time(type, name, to_add.count());
        }
    }
};
}  // namespace DMZ
