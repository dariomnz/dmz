#pragma once

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <unordered_map>

#include "Utils.hpp"
#include "driver/Driver.hpp"

namespace DMZ {
enum class StatType : int {
    Parse,
    Semantic,
    Semantic_Declarations,
    Semantic_Body,
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
    {StatType::CFG, "CFG"},
    {StatType::Codegen, "Codegen"},
    {StatType::Compile, "Compile"},
    {StatType::Run, "Run"},
    {StatType::Total, "Total"},
};
class Stats {
   public:
    struct Stat {
        StatType type;
        std::vector<Stat> subStats = {};

        void dump(size_t level, double parentTime) const {
            auto stats = Stats::instance();
            double time = stats.get_time(type);
            double percentage = time / parentTime * 100;
            std::cerr << indent_line(level, 0, true) << std::left << std::setw(20) << StatType_to_str[type];

            std::cerr << std::fixed << std::setprecision(2) << indent(2) << std::setw(5) << percentage << "%";
            std::cerr << std::fixed << std::setprecision(4) << indent(2) << std::setw(10) << time << "ms";

            std::cerr << "\n";
            for (auto&& v : subStats) {
                v.dump(level + 1, time);
            }
        }
    };

    std::vector<Stat> stat_map = {
        Stat{.type = StatType::Parse},
        Stat{.type = StatType::Semantic,
             .subStats =
                 {
                     Stat{.type = StatType::Semantic_Declarations},
                     Stat{.type = StatType::Semantic_Body},
                 }},
        Stat{.type = StatType::CFG},
        Stat{.type = StatType::Codegen},
        Stat{.type = StatType::Compile},
        Stat{.type = StatType::Run},
    };
    std::array<double, static_cast<size_t>(StatType::size)> stat_array = {};

   public:
    void dump() {
        for (auto&& v : stat_map) {
            v.dump(0, get_time(StatType::Total));
        }
    }

    void add_time(StatType t, double time) { stat_array[static_cast<size_t>(t)] += time; }

    double get_time(StatType t) { return stat_array[static_cast<size_t>(t)]; }

    static Stats& instance() {
        static Stats s;
        return s;
    }
};
#define __line2_ScopedTimer(type, line)            \
    ptr<__ScopedTimer> st##line;                   \
    if (Driver::instance().m_options.printStats) { \
        st##line = makePtr<__ScopedTimer>(type);   \
    }
#define __line1_ScopedTimer(type, line) __line2_ScopedTimer(type, line)
#define ScopedTimer(type)               __line1_ScopedTimer(type, __LINE__)

class __ScopedTimer {
   private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    StatType type;

   public:
    __ScopedTimer(StatType t) : type(t) { start = std::chrono::high_resolution_clock::now(); }
    ~__ScopedTimer() {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> to_add = now - start;
        Stats::instance().add_time(type, to_add.count());
    }
};
}  // namespace DMZ
