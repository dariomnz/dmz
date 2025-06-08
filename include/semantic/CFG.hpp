#pragma once

#include "DMZPCH.hpp"

#include "semantic/Constexpr.hpp"
#include "DMZPCHSymbols.hpp"

namespace DMZ {

struct BasicBlock {
    std::set<std::pair<int, bool>> predecessors;
    std::set<std::pair<int, bool>> successors;
    std::vector<const ResolvedStmt *> statements;
};

struct CFG {
    std::vector<BasicBlock> m_basicBlocks;
    int entry = -1;
    int exit = -1;

    int insert_new_block() {
        m_basicBlocks.emplace_back();
        return m_basicBlocks.size() - 1;
    };

    int insert_new_block_before(int before, bool reachable) {
        int b = insert_new_block();
        insert_edge(b, before, reachable);
        return b;
    }

    void insert_edge(int from, int to, bool reachable) {
        m_basicBlocks[from].successors.emplace(std::make_pair(to, reachable));
        m_basicBlocks[to].predecessors.emplace(std::make_pair(from, reachable));
    }

    void insert_stmt(const ResolvedStmt *stmt, int block) { m_basicBlocks[block].statements.emplace_back(stmt); }

    void dump() const;
};

class CFGBuilder {
    CFG cfg;
    ConstantExpressionEvaluator cee;

   public:
    CFG build(const ResolvedFunctionDecl &fn);

   private:
    int insert_block(const ResolvedBlock &block, int succ);
    int insert_stmt(const ResolvedStmt &stmt, int block);
    int insert_return_stmt(const ResolvedReturnStmt &stmt, int block);
    int insert_expr(const ResolvedExpr &expr, int block);
    int insert_if_stmt(const ResolvedIfStmt &stmt, int exit);
    int insert_while_stmt(const ResolvedWhileStmt &stmt, int exit);
    int insert_decl_stmt(const ResolvedDeclStmt &stmt, int block);
    int insert_assignment(const ResolvedAssignment &stmt, int block);
    int insert_switch_stmt(const ResolvedSwitchStmt &stmt, int block);
};
}  // namespace DMZ
