#pragma once

#include "Utils.hpp"
#include "semantic/Semantic.hpp"

namespace DMZ::lsp {

class LSPServer {
   public:
    void run();
    void stop() { m_running = false; }

   private:
    bool m_running = true;
    void handle_message(const std::string& message);
    void send_response(const std::string& id, const std::string& result);
    void send_notification(const std::string& method, const std::string& params);

    void on_initialize(const std::string& id, const std::string& params);
    void on_shutdown(const std::string& id);
    void on_exit();
    void on_did_open(const std::string& params);
    void on_did_change(const std::string& params);
    void on_did_close(const std::string& params);
    void on_definition(const std::string& id, const std::string& params);
    void on_hover(const std::string& id, const std::string& params);
    void on_semantic_tokens(const std::string& id, const std::string& params);
    void on_semantic_tokens_range(const std::string& id, const std::string& params);
    void on_completion(const std::string& id, const std::string& params);

    void collect_member_completions(const ResolvedStructDecl* decl, std::stringstream& items, bool& has_items);
    void collect_module_completions(const ResolvedModuleDecl* decl, std::stringstream& items, bool& has_items);
    void collect_completions_from_type(const ResolvedType* type, std::stringstream& items, bool& has_items);
    const ResolvedType* find_incomplete_member_base_type(const ResolvedModuleDecl* mainModule, const std::string& file,
                                                         size_t line);
    const ResolvedScope* find_scope_at_position(const ResolvedModuleDecl* mainModule, const std::string& file,
                                                size_t line, size_t col);
    void collect_scope_completions(const ResolvedScope* scope, size_t cursor_line, std::stringstream& items,
                                   bool& has_items);

    void publish_diagnostics(const std::string& filename, const std::vector<SourceLocation>& errors,
                             const std::vector<std::string>& messages);
    void process_file(const std::string& filename, const std::string& source);

    struct CacheEntry {
        ptr<ResolvedModuleDecl> module;
        std::filesystem::file_time_type last_write_time;
        std::string source;
    };

    void dump_cache();
    void dump_documents();
    void invalidate_module(const std::string& path);
    void invalidate_module(const std::string& path, std::unordered_set<std::string>& invalidated);

    struct Document {
        ResolvedModuleDecl* mainModule = nullptr;
        std::vector<SourceLocation> errors;
        std::vector<std::string> messages;
    };

    std::unordered_map<std::string, Document> m_documents;
    std::unordered_map<std::string, CacheEntry> m_module_cache;
    std::string m_std_path;
};

}  // namespace DMZ::lsp
