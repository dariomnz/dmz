

#include "driver/Driver.hpp"

namespace DMZ {
inline std::string_view trim(const std::string_view& s) {
    size_t first = s.find_first_not_of(" \t\n\r\f\v");
    if (std::string_view::npos == first) {
        return s;
    }
    size_t last = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(first, (last - first + 1));
}

Driver::Type_Sources Driver::find_modules(const Type_Sources& includeDirs,
                                          const std::unordered_set<std::string_view>& importedModuleIDs) {
    std::mutex found_modules_mutex;
    std::vector<std::filesystem::path> found_modules;

    const std::string_view module_keyword = "module";
    const char semicolon_char = ';';

    for (const auto& includeDir : includeDirs) {
        if (!std::filesystem::exists(includeDir) || !std::filesystem::is_directory(includeDir)) {
            std::cerr << "Warning: The include directory is invalid or does not exist: " << includeDir << std::endl;
            continue;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(includeDir)) {
            if (std::filesystem::is_regular_file(entry.status())) {
                if (entry.path().extension() == ".dmz") {
                    

                    // m_workers.submit([&, path = entry.path()]() {
                    std::string line_buffer;
                    std::ifstream file(entry.path());
                    if (file.is_open()) {
                        while (std::getline(file, line_buffer)) {
                            std::string_view line(line_buffer);
                            size_t module_pos = line.find(module_keyword);

                            if (module_pos != std::string::npos &&
                                (module_pos == 0 || std::isspace(line[module_pos - 1]))) {
                                size_t semicolon_pos = line.find(semicolon_char, module_pos + module_keyword.length());

                                if (semicolon_pos != std::string::npos) {
                                    std::string_view potential_name_with_spaces =
                                        line.substr(module_pos + module_keyword.length(),
                                                    semicolon_pos - (module_pos + module_keyword.length()));

                                    std::string_view trimmed_name = trim(potential_name_with_spaces);

                                    if (importedModuleIDs.find(trimmed_name) != importedModuleIDs.end()) {
                                        std::unique_lock lock(found_modules_mutex);
                                        found_modules.push_back(std::filesystem::canonical(entry.path()));
                                        break;
                                    }
                                }
                            }
                        }
                        file.close();
                    } else {
                        std::unique_lock lock(found_modules_mutex);
                        std::cerr << "Warning: The included file could not be opened for reading: " << entry.path()
                                  << std::endl;
                    }
                    // });
                }
            }
        }
    }
    // m_workers.wait();
    return found_modules;
}
}  // namespace DMZ
