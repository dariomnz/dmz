#pragma once

#include "parser/ParserSymbols.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ::lsp {

class NodeFinder {
   public:
    NodeFinder(const std::string& target_file, size_t line, size_t col);
    const ResolvedDecl* found_decl = nullptr;

    void find_in_module(const ResolvedModuleDecl& module);
    void find_in_decl(const ResolvedDecl& decl);
    void find_in_stmt(const ResolvedStmt& stmt);
    void find_in_expr(const ResolvedExpr& expr);

   private:
    void find_in_type(const ResolvedType& type);
    bool is_at_location(const SourceLocation& loc, size_t length = 0) const;
    std::string m_target_file;
    size_t m_line, m_col;
};

}  // namespace DMZ::lsp
