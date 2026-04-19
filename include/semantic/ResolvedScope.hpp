#pragma once

#include "DMZPCH.hpp"
#include "Utils.hpp"
#include "UtilsPtr.hpp"

namespace DMZ {

struct ResolvedDecl;
struct ResolvedDeferStmt;

struct ResolvedScope {
    std::unordered_map<std::string, ResolvedDecl *> table;
    std::vector<ResolvedDeferStmt *> defers;
    ResolvedScope *parent = nullptr;

    ResolvedScope(ResolvedScope *parent) : parent(parent) {}

    void merge(ptr<ResolvedScope> other, ResolvedScope *newParent) {
        parent = newParent;
        // Insert but make a dmz_unreachable if exits
        for (auto &&[key, value] : other->table) {
            if (table.count(key)) {
                dmz_unreachable(SourceLocation{}, "symbol '" + key + "' already exists in scope");
            }
            table.insert({key, value});
        }
        defers.insert(defers.end(), other->defers.begin(), other->defers.end());
    }
};

}  // namespace DMZ
