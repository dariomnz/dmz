#pragma once

#include <string>
#include <vector>

#include "parser/ParserSymbols.hpp"
#include "semantic/SemanticSymbols.hpp"

namespace DMZ::lsp {

enum class SemanticTokenType {
    Type = 0,
    Function = 1,
    Parameter = 2,
    Variable = 3,
    Property = 4,
    Namespace = 5,
};

enum class SemanticTokenModifier {
    None = 0,
    Declaration = 1 << 0,
};

struct SemanticToken {
    size_t line;
    size_t col;
    size_t length;
    SemanticTokenType type;
    uint32_t modifiers;
};

class SemanticTokensCollector {
   public:
    SemanticTokensCollector(const std::string& target_file, const std::string& source = "");
    std::vector<SemanticToken> collect(const std::vector<ptr<ResolvedModuleDecl>>& resolvedAST);

   private:
    void traverse_module(const ResolvedModuleDecl& module);
    void traverse_decl(const ResolvedDecl& decl);
    void traverse_stmt(const ResolvedStmt& stmt);
    void traverse_expr(const ResolvedExpr& expr);
    void traverse_type(const ResolvedType& type);

    void add_token(const SourceLocation& loc, std::string_view identifier, SemanticTokenType type,
                   uint32_t modifiers = (uint32_t)SemanticTokenModifier::None);

    std::string m_target_file;
    std::string m_source;
    std::vector<SemanticToken> m_tokens;
};

}  // namespace DMZ::lsp
