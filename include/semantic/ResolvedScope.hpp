#pragma once

#include "Utils.hpp"
#include "UtilsPtr.hpp"

namespace DMZ {

struct ResolvedDecl;
struct ResolvedDeferStmt;
struct ResolvedModuleDecl;
struct ResolvedStructDecl;
struct ResolvedFuncDecl;

struct ResolvedScope {
    std::unordered_map<std::string, ResolvedDecl *> table;
    std::vector<ResolvedDeferStmt *> defers;
    ResolvedScope *parent = nullptr;
    ResolvedModuleDecl *currentModule = nullptr;
    ResolvedFuncDecl *currentFunction = nullptr;
    ResolvedStructDecl *currentStruct = nullptr;

    ResolvedScope(ResolvedScope *parent)
        : parent(parent),
          currentModule(parent ? parent->currentModule : nullptr),
          currentFunction(parent ? parent->currentFunction : nullptr),
          currentStruct(parent ? parent->currentStruct : nullptr) {}
    ResolvedScope(ResolvedModuleDecl *module) : parent(nullptr), currentModule(module) {}

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
