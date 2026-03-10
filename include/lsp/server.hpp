#pragma once

#include <unordered_map>
#include <vector>

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "Utils.hpp"
#include "parser/ParserSymbols.hpp"
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
    void on_definition(const std::string& id, const std::string& params);
    void on_hover(const std::string& id, const std::string& params);
    void on_semantic_tokens(const std::string& id, const std::string& params);
    void on_completion(const std::string& id, const std::string& params);

    void collect_member_completions(const ResolvedStructDecl* decl, std::stringstream& items, bool& has_items);
    void collect_module_completions(const ResolvedModuleDecl* decl, std::stringstream& items, bool& has_items);
    void collect_completions_from_type(const ResolvedType* type, std::stringstream& items, bool& has_items);
    const ResolvedType* find_incomplete_member_base_type(const std::vector<ptr<ResolvedModuleDecl>>& ast,
                                                         const std::string& file, size_t line);

    void publish_diagnostics(const std::string& filename, const std::vector<SourceLocation>& errors,
                             const std::vector<std::string>& messages);
    void process_file(const std::string& filename, const std::string& source);

    struct Document {
        std::string source;
        ptr<ModuleDecl> ast;
        ptr<Sema> sema;
        std::vector<ptr<ResolvedModuleDecl>> resolvedAST;
    };

    std::unordered_map<std::string, Document> m_documents;
    std::string m_std_path;
};

}  // namespace DMZ::lsp
