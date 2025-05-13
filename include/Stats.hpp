#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace DMZ {
class Stats {
   public:
    enum type : int {
        parseTime = 0,
        semanticTime,
        semanticResolveStructsTime,
        semanticResolveFunctionsTime,
        semanticResolveBodysTime,
        CFGTime,
        codegenTime,
        compileTime,
        runTime,
        total,
        size,
    };

   private:
    struct dump_value {
        type t;
        dump_value(type t) : t(t) {}

        friend std::ostream& operator<<(std::ostream& os, const dump_value& val) {
            int64_t time = Stats::instance().times[val.t];
            int64_t total_time = Stats::instance().times[type::total];
            double percentage = (double)time / (double)total_time * 100.0;
            os << std::fixed << std::setprecision(2) << std::setw(6) << percentage << " % ";
            os << std::fixed << std::setprecision(6) << std::setw(10) << (time * 0.000000001) << " s";

            return os;
        }
    };

   public:
    std::array<std::atomic_int64_t, static_cast<size_t>(type::size)> times;
    void dump() {
        times[type::total] = 0;
        for (int i = 0; i < type::total; i++) {
            times[type::total] += times[i];
        }
        std::stringstream out;
        out << "Parse time    " << dump_value(type::parseTime) << std::endl;
        out << "Semantic time " << dump_value(type::semanticTime) << std::endl;
        out << "  Struct decl " << dump_value(type::semanticResolveStructsTime) << std::endl;
        out << "  Func decl   " << dump_value(type::semanticResolveFunctionsTime) << std::endl;
        out << "  Bodys decl  " << dump_value(type::semanticResolveBodysTime) << std::endl;
        out << "CFG  time     " << dump_value(type::CFGTime) << std::endl;
        out << "Codegen time  " << dump_value(type::codegenTime) << std::endl;
        out << "Compile time  " << dump_value(type::compileTime) << std::endl;
        out << "Run time      " << dump_value(type::runTime) << std::endl;
        out << "Total         " << dump_value(type::total) << std::endl;

        std::cerr << out.str();
    }

    void add_time(type t, int64_t time) { times[t] += time; }

    static Stats& instance() {
        static Stats s;
        return s;
    }
};

class ScopedTimer {
   private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    Stats::type type;

   public:
    ScopedTimer(Stats::type t) : type(t) { start = std::chrono::high_resolution_clock::now(); }
    ~ScopedTimer() {
        auto now = std::chrono::high_resolution_clock::now();
        auto to_add = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
        Stats::instance().add_time(type, to_add);
    }
};
}  // namespace DMZ
