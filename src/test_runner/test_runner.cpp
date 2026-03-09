#include "test_runner/test_runner.hpp"

#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Debug.hpp"

namespace fs = std::filesystem;

namespace DMZ {

enum class CheckKind { Check, CheckNext, CheckNot };

struct CheckDirective {
    CheckKind kind;
    std::string pattern;
    int line_num;
};

struct TestCase {
    fs::path path;
    std::vector<std::string> run_lines;
    std::vector<CheckDirective> checks;
};

struct ExecResult {
    std::string output;
    int status;
};

struct TestResult {
    bool success;
    std::string test_name;
    double elapsed;
    std::vector<std::string> errors;
    std::string system_error;
    std::string output;
};

static std::string get_executable_path() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::string(result, (count > 0) ? count : 0);
}

static std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

ExecResult exec(const std::string& cmd) {
    std::array<char, 1024> buffer;
    std::string result;
    debug_msg("Comand to run: '" << cmd << "'");
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    int status = pclose(pipe);
    return {result, status};
}

static void trim(std::string& s) {
    if (s.empty()) return;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    if (s.empty()) return;
    size_t last = s.find_last_not_of(" \t\r\n");
    if (last != std::string::npos)
        s.erase(last + 1);
    else
        s.clear();
}

static std::string normalize_whitespace(std::string s) {
    trim(s);
    if (s.empty()) return "";
    std::string res;
    bool last_was_ws = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!last_was_ws) {
                res += ' ';
                last_was_ws = true;
            }
        } else {
            res += c;
            last_was_ws = false;
        }
    }
    return res;
}

static std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::stringstream ss(s);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static bool match_pattern(const std::string& text, const std::string& pattern, size_t& match_pos, size_t& match_len) {
    std::string n_pat = normalize_whitespace(pattern);
    std::string n_text = normalize_whitespace(text);

    if (n_pat.empty()) {
        if (n_text.empty()) {
            match_pos = 0;
            match_len = text.length();
            return true;
        }
        return false;
    }

    size_t last_pos = 0;
    size_t pat_pos = 0;
    while (true) {
        size_t glob = n_pat.find("{{.*}}", pat_pos);
        if (glob == std::string::npos) {
            std::string remaining = n_pat.substr(pat_pos);
            if (n_text.substr(last_pos).find(remaining) == std::string::npos) return false;
            match_pos = 0;
            match_len = text.length();
            return true;
        }

        std::string part = n_pat.substr(pat_pos, glob - pat_pos);
        if (!part.empty()) {
            size_t found = n_text.find(part, last_pos);
            if (found == std::string::npos) return false;
            last_pos = found + part.length();
        }
        pat_pos = glob + 6;
    }
}

static std::pair<bool, std::string> verify_checks(const std::string& output,
                                                  const std::vector<CheckDirective>& checks) {
    auto lines = split_lines(output);
    size_t current_line_idx = 0;
    size_t current_col_idx = 0;
    bool has_matched_once = false;

    for (size_t i = 0; i < checks.size(); ++i) {
        const auto& check = checks[i];

        if (check.kind == CheckKind::Check) {
            bool found = false;
            for (size_t l = current_line_idx; l < lines.size(); ++l) {
                size_t start_col = (l == current_line_idx) ? current_col_idx : 0;
                if (start_col >= lines[l].length() && !lines[l].empty()) continue;

                std::string search_text = lines[l].substr(start_col);
                size_t m_pos, m_len;
                if (match_pattern(search_text, check.pattern, m_pos, m_len)) {
                    current_line_idx = l;
                    current_col_idx = start_col + m_pos + m_len;
                    found = true;
                    has_matched_once = true;
                    break;
                }
            }
            if (!found)
                return {false, "CHECK: '" + check.pattern + "' not found (source line " +
                                   std::to_string(check.line_num) + ")"};
        } else if (check.kind == CheckKind::CheckNext) {
            if (!has_matched_once)
                return {false,
                        "CHECK-NEXT: used before any CHECK (source line " + std::to_string(check.line_num) + ")"};
            size_t next_line = current_line_idx + 1;
            if (next_line >= lines.size())
                return {false,
                        "CHECK-NEXT: reached end of output (source line " + std::to_string(check.line_num) + ")"};

            size_t m_pos, m_len;
            if (!match_pattern(lines[next_line], check.pattern, m_pos, m_len)) {
                return {false, "CHECK-NEXT: '" + check.pattern + "' not found on next line (got: '" + lines[next_line] +
                                   "') (source line " + std::to_string(check.line_num) + ")"};
            }
            current_line_idx = next_line;
            current_col_idx = m_pos + m_len;
        } else if (check.kind == CheckKind::CheckNot) {
            size_t end_line = lines.size();
            size_t end_col = 0;
            for (size_t j = i + 1; j < checks.size(); j++) {
                if (checks[j].kind != CheckKind::CheckNot) {
                    size_t temp_line = current_line_idx;
                    size_t temp_col = current_col_idx;
                    for (size_t l = temp_line; l < lines.size(); l++) {
                        size_t start_col = (l == temp_line) ? temp_col : 0;
                        size_t m_pos, m_len;
                        if (match_pattern(lines[l].substr(start_col), checks[j].pattern, m_pos, m_len)) {
                            end_line = l;
                            end_col = start_col + m_pos;
                            break;
                        }
                    }
                    break;
                }
            }

            for (size_t l = current_line_idx; l <= end_line && l < lines.size(); l++) {
                size_t start = (l == current_line_idx) ? current_col_idx : 0;
                size_t end = (l == end_line) ? end_col : lines[l].length();
                if (start >= lines[l].length() && !lines[l].empty()) continue;
                std::string search_range = lines[l].substr(start, end - start);
                size_t m_pos, m_len;
                if (match_pattern(search_range, check.pattern, m_pos, m_len)) {
                    return {false, "CHECK-NOT: '" + check.pattern + "' found but excluded (source line " +
                                       std::to_string(check.line_num) + ")"};
                }
            }
        }
    }
    return {true, ""};
}

static std::string process_line_vars(std::string pat, int line_num) {
    size_t pos = 0;
    while ((pos = pat.find("[[#", pos)) != std::string::npos) {
        size_t end = pat.find("]]", pos);
        if (end == std::string::npos) break;
        std::string expr = pat.substr(pos + 3, end - (pos + 3));
        trim(expr);
        if (expr.find("@LINE") != std::string::npos) {
            long long val = line_num;
            size_t plus = expr.find('+');
            size_t minus = expr.find('-');
            if (plus != std::string::npos) {
                try {
                    val += std::stoll(expr.substr(plus + 1));
                } catch (...) {
                }
            } else if (minus != std::string::npos) {
                try {
                    val -= std::stoll(expr.substr(minus + 1));
                } catch (...) {
                }
            }
            std::string replacement = std::to_string(val);
            pat.replace(pos, end + 2 - pos, replacement);
            pos += replacement.length();
        } else {
            pos = end + 2;
        }
    }
    return pat;
}

TestResult perform_test(const std::string& dmz_bin, const TestCase& tc) {
    auto start = std::chrono::high_resolution_clock::now();
    try {
        if (tc.run_lines.empty()) return {false, tc.path.string(), 0, {}, "NO RUN LINE FOUND", ""};

        std::string abs_path = fs::absolute(tc.path).string();
        std::string abs_dir = fs::absolute(tc.path.parent_path()).string();

        for (size_t i = 0; i < tc.run_lines.size(); ++i) {
            std::string cmd = tc.run_lines[i];
            bool originally_had_filecheck = (cmd.find("| filecheck") != std::string::npos);

            bool should_check = originally_had_filecheck;
            if (!should_check && !tc.checks.empty()) {
                if (i == 0 && cmd.find("dmz") != std::string::npos && cmd.find("diff") == std::string::npos) {
                    should_check = true;
                }
            }

            if (should_check) {
                size_t pipe_pos = cmd.find("| filecheck");
                if (pipe_pos != std::string::npos) cmd = cmd.substr(0, pipe_pos);
                trim(cmd);
                if (cmd.size() > 7 && cmd.substr(cmd.size() - 7) == "|| true") {
                    cmd = cmd.substr(0, cmd.size() - 7);
                    trim(cmd);
                }
                while (cmd.size() >= 2 && cmd.front() == '(' && cmd.back() == ')') {
                    cmd = cmd.substr(1, cmd.size() - 2);
                    trim(cmd);
                }
            }

            auto replace_dmz_cmd = [&](std::string& s, const std::string& replacement) {
                size_t pos = 0;
                while ((pos = s.find("dmz", pos)) != std::string::npos) {
                    bool before = (pos == 0 || (!std::isalnum(s[pos - 1]) && s[pos - 1] != '_' && s[pos - 1] != '/' &&
                                                s[pos - 1] != '.'));
                    bool after = (pos + 3 == s.length() || (!std::isalnum(s[pos + 3]) && s[pos + 3] != '_'));
                    if (before && after) {
                        s.replace(pos, 3, replacement);
                        pos += replacement.length();
                    } else {
                        pos += 3;
                    }
                }
            };
            replace_dmz_cmd(cmd, dmz_bin);
            cmd = replace_all(cmd, "%s", abs_path);
            cmd = replace_all(cmd, "%S", abs_dir);

            if (should_check && cmd.find("2>&1") == std::string::npos) cmd += " 2>&1";

            std::string escaped_cmd = cmd;
            escaped_cmd = replace_all(escaped_cmd, "\"", "\\\"");
            escaped_cmd = replace_all(escaped_cmd, "$", "\\$");
            std::string full_cmd = "timeout 1s bash -c \"" + escaped_cmd + "\"";
            ExecResult res = exec(full_cmd);
            int exit_code = WEXITSTATUS(res.status);

            if (should_check) {
                auto [passed, err_msg] = verify_checks(res.output, tc.checks);
                if (!passed) {
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> elapsed = end - start;
                    std::vector<std::string> errors;
                    errors.push_back("Verification failed: " + err_msg);
                    errors.push_back("Command: " + cmd);
                    return {false, tc.path.string(), elapsed.count(), errors, "", res.output};
                }
            } else if (exit_code != 0) {
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = end - start;
                if (exit_code == 124) return {false, tc.path.string(), elapsed.count(), {}, "TIMEOUT", res.output};
                std::vector<std::string> errors;
                errors.push_back("Command failed with exit code " + std::to_string(exit_code));
                errors.push_back("Command: " + cmd);
                return {false, tc.path.string(), elapsed.count(), errors, "", res.output};
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        return {true, tc.path.string(), elapsed.count(), {}, "", ""};
    } catch (const std::exception& e) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        return {false, tc.path.string(), elapsed.count(), {}, e.what(), ""};
    }
}

int run_tests(std::string_view test_path, const TestOptions& options) {
    auto start = std::chrono::high_resolution_clock::now();
    fs::path target = test_path;
    std::string dmz_bin = options.binary_path;
    if (dmz_bin.empty()) dmz_bin = get_executable_path();
    if (dmz_bin.empty() || !fs::exists(dmz_bin)) {
        if (fs::exists("./build/bin/dmz"))
            dmz_bin = fs::absolute("./build/bin/dmz").string();
        else if (fs::exists("./bin/dmz"))
            dmz_bin = fs::absolute("./bin/dmz").string();
    }

    std::vector<TestCase> tests;
    auto process_file = [&](const fs::path& p) {
        if (p.extension() == ".dmz") {
            TestCase tc;
            tc.path = p;
            std::ifstream file(p);
            std::string line;
            int line_num = 0;
            while (std::getline(file, line)) {
                line_num++;
                if (size_t pos = line.find("// CHECK-NEXT:"); pos != std::string::npos) {
                    std::string pat = line.substr(pos + 14);
                    pat = process_line_vars(pat, line_num);
                    tc.checks.push_back({CheckKind::CheckNext, pat, line_num});
                } else if (size_t pos = line.find("// CHECK-NOT:"); pos != std::string::npos) {
                    std::string pat = line.substr(pos + 13);
                    pat = process_line_vars(pat, line_num);
                    tc.checks.push_back({CheckKind::CheckNot, pat, line_num});
                } else if (size_t pos = line.find("// CHECK:"); pos != std::string::npos) {
                    std::string pat = line.substr(pos + 9);
                    pat = process_line_vars(pat, line_num);
                    tc.checks.push_back({CheckKind::Check, pat, line_num});
                } else if (size_t pos = line.find("// RUN:"); pos != std::string::npos) {
                    std::string val = line.substr(pos + 7);
                    trim(val);
                    if (!val.empty()) tc.run_lines.push_back(val);
                }
            }
            if (!tc.run_lines.empty()) {
                tests.push_back(tc);
            }
        }
    };

    if (fs::is_directory(target)) {
        for (const auto& entry : fs::recursive_directory_iterator(target))
            if (entry.is_regular_file()) process_file(entry.path());
    } else
        process_file(target);

    if (tests.empty()) {
        std::cout << "No tests found in " << target << std::endl;
        return 0;
    }
    std::sort(tests.begin(), tests.end(), [](const TestCase& a, const TestCase& b) { return a.path < b.path; });

    std::mutex output_mutex;
    std::mutex queue_mutex;
    std::queue<size_t> test_queue;
    for (size_t i = 0; i < tests.size(); ++i) test_queue.push(i);

    std::atomic<int> passed(0);
    int num_workers = options.parallel_jobs;
    if (num_workers <= 0) num_workers = std::thread::hardware_concurrency();
    if (num_workers <= 0) num_workers = 1;

    auto worker_task = [&] {
        while (true) {
            size_t test_idx;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (test_queue.empty()) return;
                test_idx = test_queue.front();
                test_queue.pop();
            }
            auto result = perform_test(dmz_bin, tests[test_idx]);
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                if (result.success) {
                    passed++;
                    if (!options.quiet)
                        std::cout << "Running test: " << result.test_name << "... " << "\033[32mPASSED\033[0m in "
                                  << std::fixed << std::setprecision(3) << result.elapsed << " seconds" << std::endl;
                } else {
                    std::cout << "Running test: " << result.test_name << "... " << "\033[31mFAILED\033[0m in "
                              << std::fixed << std::setprecision(3) << result.elapsed << " seconds" << std::endl;
                    if (!result.system_error.empty())
                        std::cout << "  - \033[31mERROR:\033[0m " << result.system_error << std::endl;
                    for (const auto& err : result.errors) std::cout << "  - " << err << std::endl;
                    if (!result.output.empty()) {
                        std::cout << "  --- PROGRAM OUTPUT ---" << std::endl;
                        std::string line;
                        std::stringstream ss(result.output);
                        if (result.output.empty())
                            std::cout << "  | (empty output)" << std::endl;
                        else
                            while (std::getline(ss, line)) std::cout << "  | " << line << std::endl;
                        std::cout << "  ----------------------" << std::endl;
                    }
                }
            }
        }
    };

    if (num_workers == 1)
        worker_task();
    else {
        std::vector<std::thread> workers;
        for (int i = 0; i < num_workers; ++i) workers.emplace_back(worker_task);
        for (auto& w : workers) w.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "\nSummary: " << passed << "/" << tests.size() << " tests passed in " << std::fixed
              << std::setprecision(3) << elapsed.count() << " seconds" << std::endl;
    return (passed == (int)tests.size()) ? 0 : 1;
}

}  // namespace DMZ
