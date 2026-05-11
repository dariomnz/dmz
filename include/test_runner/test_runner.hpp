#pragma once

#include <string>

namespace DMZ {

struct TestOptions {
    int parallel_jobs = 1;
    bool quiet = false;
    bool fail_fast = false;
    bool patch = false;
    std::string binary_path;
};

int run_tests(std::string_view test_path, const TestOptions& options = {});
}  // namespace DMZ
