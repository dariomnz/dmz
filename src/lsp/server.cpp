#include "lsp/server.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include "driver/Driver.hpp"
#include "lexer/Lexer.hpp"
#include "lsp/node_finder.hpp"
#include "lsp/protocol.hpp"
#include "lsp/semantic_tokens.hpp"
#include "parser/Parser.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ::lsp {

void LSPServer::run() {
    std::cerr << "[LSP] Server started, waiting for messages..." << std::endl;
    while (m_running && std::cin) {
        std::string line;
        if (!std::getline(std::cin, line)) break;

        if (line.starts_with("Content-Length: ")) {
            int length = std::stoi(line.substr(16));

            while (std::getline(std::cin, line) && !line.empty() && line != "\r") {
            }

            std::string body(length, ' ');
            std::cin.read(&body[0], length);
            std::cerr << "[LSP] Received: " << body << std::endl;
            handle_message(body);
        }
    }
}

void LSPServer::handle_message(const std::string& message) {
    std::string id = get_json_value(message, "id");
    std::string method = get_json_value(message, "method");
    std::cerr << "[LSP] Handling method: " << method << " (id: " << id << ")" << std::endl;

    if (method == "initialize") {
        on_initialize(id, message);
    } else if (method == "shutdown") {
        on_shutdown(id);
    } else if (method == "exit") {
        on_exit();
    } else if (method == "textDocument/didOpen") {
        on_did_open(message);
    } else if (method == "textDocument/didChange") {
        on_did_change(message);
    } else if (method == "textDocument/definition") {
        on_definition(id, message);
    } else if (method == "textDocument/hover") {
        on_hover(id, message);
    } else if (method == "textDocument/semanticTokens/full") {
        on_semantic_tokens(id, message);
    } else if (method == "textDocument/completion") {
        on_completion(id, message);
    }
}

void LSPServer::send_response(const std::string& id, const std::string& result) {
    std::string body = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + result + "}";
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body << std::flush;
    std::cerr << "[LSP] Sent response: " << body << std::endl;
}

void LSPServer::send_notification(const std::string& method, const std::string& params) {
    std::string body = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params + "}";
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body << std::flush;
    std::cerr << "[LSP] Sent notification: " << body << std::endl;
}

void LSPServer::on_initialize(const std::string& id, const std::string& params) {
    (void)params;

    // Detect project root from initialization parameters if possible
    std::string rootUri = get_json_value(params, "rootUri");
    if (rootUri.empty()) rootUri = get_json_value(params, "rootPath");  // Some clients use rootPath

    if (rootUri.starts_with("file://")) {
        std::string rootPath = rootUri.substr(7);
        std::filesystem::path stdPath = std::filesystem::path(rootPath) / "std" / "std.dmz";
        if (std::filesystem::exists(stdPath)) {
            m_std_path = std::filesystem::canonical(stdPath).string();
        }
    }

    if (m_std_path.empty()) {
        // Fallback to current working directory
        std::filesystem::path stdPath = std::filesystem::absolute("std/std.dmz");
        if (std::filesystem::exists(stdPath)) {
            m_std_path = std::filesystem::canonical(stdPath).string();
        }
    }

    if (!m_std_path.empty()) {
        std::cerr << "[LSP] Found std at: " << m_std_path << std::endl;
    }

    send_response(id,
                  "{\"capabilities\":{"
                  "\"textDocumentSync\":1,"
                  "\"definitionProvider\":true,"
                  "\"hoverProvider\":true,"
                  "\"semanticTokensProvider\":{"
                  "\"legend\":{"
                  "\"tokenTypes\":[\"type\",\"function\",\"parameter\",\"variable\",\"property\",\"namespace\"],"
                  "\"tokenModifiers\":[\"declaration\"]"
                  "},"
                  "\"full\":true"
                  "},"
                  "\"completionProvider\":{"
                  "\"resolveProvider\":false,"
                  "\"triggerCharacters\":[\".\"]"
                  "}"
                  "}}");
}

void LSPServer::on_shutdown(const std::string& id) {
    std::cerr << "[LSP] Received shutdown request." << std::endl;
    send_response(id, "null");
}

void LSPServer::on_exit() {
    std::cerr << "[LSP] Received exit notification. Exiting..." << std::endl;
    m_running = false;
}

void LSPServer::on_did_open(const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    std::string path = uri;
    if (path.starts_with("file://")) path = path.substr(7);

    std::string text = get_json_value(params, "text");
    if (!text.empty()) {
        process_file(path, text);
        return;
    }

    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        process_file(path, ss.str());
    }
}

void LSPServer::on_did_change(const std::string& params) { on_did_open(params); }

void LSPServer::on_definition(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    std::string line_str = get_json_value(params, "line");
    std::string char_str = get_json_value(params, "character");

    if (line_str.empty() || char_str.empty()) {
        send_response(id, "null");
        return;
    }

    if (m_documents.find(uri) == m_documents.end()) {
        send_response(id, "null");
        return;
    }

    auto& doc = m_documents[uri];
    if (doc.resolvedAST.empty()) {
        send_response(id, "null");
        return;
    }

    size_t line = std::stoul(line_str) + 1;
    size_t col = std::stoul(char_str);

    NodeFinder finder(uri, line, col);
    for (const auto& mod : doc.resolvedAST) {
        finder.find_in_module(*mod);
        if (finder.found_decl) break;
    }

    if (finder.found_decl) {
        std::cerr << "[LSP] Definition found: " << finder.found_decl->identifier << " at "
                  << finder.found_decl->location << std::endl;
        const auto& loc = finder.found_decl->location;
        std::stringstream ss;
        ss << "{\"uri\":\"file://" << loc.file_name << "\",\"range\":{"
           << "\"start\":{\"line\":" << (loc.line - 1) << ",\"character\":" << loc.col << "},"
           << "\"end\":{\"line\":" << (loc.line - 1)
           << ",\"character\":" << (loc.col + finder.found_decl->identifier.length()) << "}"
           << "}}";
        send_response(id, ss.str());
    } else {
        std::cerr << "[LSP] Definition not found at " << uri << ":" << line << ":" << col << std::endl;
        send_response(id, "null");
    }
}

void LSPServer::on_hover(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    std::string line_str = get_json_value(params, "line");
    std::string char_str = get_json_value(params, "character");

    if (line_str.empty() || char_str.empty() || m_documents.find(uri) == m_documents.end()) {
        send_response(id, "null");
        return;
    }

    auto& doc = m_documents[uri];
    size_t line = std::stoul(line_str) + 1;
    size_t col = std::stoul(char_str);

    NodeFinder finder(uri, line, col);
    for (const auto& mod : doc.resolvedAST) {
        finder.find_in_module(*mod);
        if (finder.found_decl) break;
    }

    if (finder.found_decl) {
        std::stringstream ss;
        ss << "{\"contents\":{\"kind\":\"markdown\",\"value\":\"```dmz\\n"
           << escape_json(finder.found_decl->identifier) << ": " << escape_json(finder.found_decl->type->to_str())
           << "\\n```\"}}";
        send_response(id, ss.str());
    } else {
        send_response(id, "null");
    }
}

void LSPServer::on_semantic_tokens(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    if (m_documents.find(uri) == m_documents.end()) {
        send_response(id, "{\"data\":[]}");
        return;
    }

    auto& doc = m_documents[uri];
    if (doc.resolvedAST.empty()) {
        send_response(id, "{\"data\":[]}");
        return;
    }

    SemanticTokensCollector collector(uri, doc.source);
    std::vector<SemanticToken> tokens = collector.collect(doc.resolvedAST);

    std::stringstream ss;
    ss << "{\"data\":[";
    size_t lastLine = 0;
    size_t lastChar = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        size_t deltaLine = t.line - lastLine;
        size_t deltaChar = (deltaLine == 0) ? (t.col - lastChar) : t.col;

        ss << deltaLine << "," << deltaChar << "," << t.length << "," << (int)t.type << "," << (int)t.modifiers;
        if (i < tokens.size() - 1) ss << ",";

        lastLine = t.line;
        lastChar = t.col;
    }
    ss << "]}";
    send_response(id, ss.str());
}

void LSPServer::collect_member_completions(const ResolvedStructDecl* decl, std::stringstream& items, bool& has_items) {
    if (!decl) return;
    for (const auto& field : decl->fields) {
        if (has_items) items << ",";
        items << "{\"label\":\"" << escape_json(field->identifier) << "\",\"kind\":5,\"detail\":\""
              << escape_json(field->type->to_str()) << "\"}";
        has_items = true;
    }
    for (const auto& method : decl->functions) {
        if (has_items) items << ",";
        items << "{\"label\":\"" << escape_json(method->identifier) << "\",\"kind\":2,\"detail\":\""
              << escape_json(method->type->to_str()) << "\"}";
        has_items = true;
    }
}

void LSPServer::collect_module_completions(const ResolvedModuleDecl* decl, std::stringstream& items, bool& has_items) {
    if (!decl) return;
    for (const auto& d : decl->declarations) {
        if (!d->isPublic) continue;
        if (dynamic_cast<const ResolvedTestDecl*>(d.get())) continue;
        if (has_items) items << ",";
        int kind = 1;   // Default
        if (dynamic_cast<const ResolvedFunctionDecl*>(d.get()))
            kind = 3;   // Function
        else if (dynamic_cast<const ResolvedStructDecl*>(d.get()) ||
                 dynamic_cast<const ResolvedTypeStructDecl*>(d->type.get()))
            kind = 22;  // Struct
        else if (dynamic_cast<const ResolvedModuleDecl*>(d.get()) ||
                 dynamic_cast<const ResolvedTypeModule*>(d->type.get()))
            kind = 9;   // Module
        else if (dynamic_cast<const ResolvedDeclStmt*>(d.get()) || dynamic_cast<const ResolvedVarDecl*>(d.get()))
            kind = 6;   // Variable
        else if (dynamic_cast<const ResolvedGenericTypeDecl*>(d.get()))
            kind = 25;  // TypeParameter

        items << "{\"label\":\"" << escape_json(d->identifier) << "\",\"kind\":" << kind << ",\"detail\":\""
              << escape_json(d->type->to_str()) << "\"}";
        has_items = true;
    }
}

void LSPServer::collect_completions_from_type(const ResolvedType* type, std::stringstream& items, bool& has_items) {
    if (!type) return;
    while (auto pt = dynamic_cast<const ResolvedTypePointer*>(type)) {
        type = pt->pointerType.get();
    }
    if (auto st = dynamic_cast<const ResolvedTypeStruct*>(type)) {
        collect_member_completions(st->decl, items, has_items);
    } else if (auto st_decl = dynamic_cast<const ResolvedTypeStructDecl*>(type)) {
        collect_member_completions(st_decl->decl, items, has_items);
    } else if (auto mt = dynamic_cast<const ResolvedTypeModule*>(type)) {
        collect_module_completions(mt->moduleDecl, items, has_items);
    }
}

// Walk the resolved AST to find ResolvedMemberExpr with empty member identifier on the target line.
// Returns the resolved type of the base expression, or nullptr if not found.
const ResolvedType* LSPServer::find_incomplete_member_base_type(const std::vector<ptr<ResolvedModuleDecl>>& ast,
                                                                const std::string& file, size_t line) {
    // Walk all expressions recursively looking for an incomplete MemberExpr on the target line
    struct IncompleteMemberFinder {
        const std::string& target_file;
        size_t target_line;
        const ResolvedType* result = nullptr;

        void visit_module(const ResolvedModuleDecl& mod) {
            for (const auto& decl : mod.declarations) visit_decl(*decl);
        }

        void visit_decl(const ResolvedDecl& decl) {
            if (result) return;
            if (const auto* fd = dynamic_cast<const ResolvedFunctionDecl*>(&decl)) {
                for (const auto& param : fd->params) visit_decl(*param);
                if (fd->body) visit_stmt(*fd->body);
            } else if (const auto* sd = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
                for (const auto& method : sd->functions) visit_decl(*method);
            } else if (const auto* vd = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
                if (vd->initializer) visit_expr(*vd->initializer);
            }
        }

        void visit_stmt(const ResolvedStmt& stmt) {
            if (result) return;
            if (const auto* block = dynamic_cast<const ResolvedBlock*>(&stmt)) {
                for (const auto& s : block->statements) visit_stmt(*s);
            } else if (const auto* ds = dynamic_cast<const ResolvedDeclStmt*>(&stmt)) {
                if (ds->varDecl) visit_decl(*ds->varDecl);
            } else if (const auto* rs = dynamic_cast<const ResolvedReturnStmt*>(&stmt)) {
                if (rs->expr) visit_expr(*rs->expr);
            } else if (const auto* is = dynamic_cast<const ResolvedIfStmt*>(&stmt)) {
                visit_expr(*is->condition);
                visit_stmt(*is->trueBlock);
                if (is->falseBlock) visit_stmt(*is->falseBlock);
            } else if (const auto* ws = dynamic_cast<const ResolvedWhileStmt*>(&stmt)) {
                visit_expr(*ws->condition);
                visit_stmt(*ws->body);
            } else if (const auto* fs = dynamic_cast<const ResolvedForStmt*>(&stmt)) {
                for (const auto& cond : fs->conditions) visit_expr(*cond);
                visit_stmt(*fs->body);
            } else if (const auto* as = dynamic_cast<const ResolvedAssignment*>(&stmt)) {
                visit_expr(*as->assignee);
                visit_expr(*as->expr);
            } else if (const auto* def = dynamic_cast<const ResolvedDeferStmt*>(&stmt)) {
                visit_stmt(*def->block);
            } else if (const auto* switchStmt = dynamic_cast<const ResolvedSwitchStmt*>(&stmt)) {
                visit_expr(*switchStmt->condition);
                for (const auto& caseStmt : switchStmt->cases) {
                    visit_stmt(*caseStmt);
                }
                if (switchStmt->elseBlock) visit_stmt(*switchStmt->elseBlock);
            } else if (const auto* expr = dynamic_cast<const ResolvedExpr*>(&stmt)) {
                visit_expr(*expr);
            }
        }

        void visit_expr(const ResolvedExpr& expr) {
            if (result) return;
            if (const auto* me = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
                if (me->member.identifier.empty() && me->location.file_name == target_file &&
                    me->location.line == target_line) {
                    result = me->base->type.get();
                    return;
                }
                visit_expr(*me->base);
            } else if (const auto* call = dynamic_cast<const ResolvedCallExpr*>(&expr)) {
                visit_expr(*call->callee);
                for (const auto& arg : call->arguments) visit_expr(*arg);
            } else if (const auto* bin = dynamic_cast<const ResolvedBinaryOperator*>(&expr)) {
                visit_expr(*bin->lhs);
                visit_expr(*bin->rhs);
            } else if (const auto* un = dynamic_cast<const ResolvedUnaryOperator*>(&expr)) {
                visit_expr(*un->operand);
            } else if (const auto* grp = dynamic_cast<const ResolvedGroupingExpr*>(&expr)) {
                visit_expr(*grp->expr);
            } else if (const auto* at = dynamic_cast<const ResolvedArrayAtExpr*>(&expr)) {
                visit_expr(*at->array);
                visit_expr(*at->index);
            }
        }
    };

    IncompleteMemberFinder finder{file, line};
    for (const auto& mod : ast) {
        finder.visit_module(*mod);
        if (finder.result) return finder.result;
    }
    return nullptr;
}

void LSPServer::on_completion(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    std::string line_str = get_json_value(params, "line");
    std::string char_str = get_json_value(params, "character");

    if (line_str.empty() || char_str.empty() || m_documents.find(uri) == m_documents.end()) {
        std::cerr << "[LSP] Completion request failed: invalid parameters." << std::endl;
        send_response(id, "{\"isIncomplete\":false,\"items\":[]}");
        return;
    }

    auto& doc = m_documents[uri];
    size_t line = std::stoul(line_str) + 1;
    size_t col = std::stoul(char_str);

    std::cerr << "[LSP] Completion at line=" << line << " col=" << col << std::endl;

    // Scan backwards from cursor to find if we're in a member completion context
    size_t current_line = 1;
    size_t current_pos = 0;
    while (current_line < line && current_pos < doc.source.length()) {
        if (doc.source[current_pos] == '\n') current_line++;
        current_pos++;
    }

    bool is_member_completion = false;
    int dot_col = -1;
    int col_iter = (int)col;
    while (col_iter >= 0 && current_pos + col_iter < doc.source.length()) {
        char c = doc.source[current_pos + col_iter];
        if (c == '.') {
            is_member_completion = true;
            dot_col = col_iter;
            break;
        }
        if (col_iter < (int)col && !std::isalnum(c) && c != '_') {
            break;
        }
        col_iter--;
    }

    std::cerr << "[LSP] is_member_completion=" << is_member_completion << " dot_col=" << dot_col << std::endl;

    std::stringstream items;
    bool has_items = false;

    if (is_member_completion) {
        // First, try to find the base type via the resolved AST (incomplete MemberExpr approach)
        const ResolvedType* baseType = find_incomplete_member_base_type(doc.resolvedAST, uri, line);
        if (baseType) {
            std::cerr << "[LSP] Found base type from incomplete MemberExpr: " << baseType->to_str() << std::endl;
            collect_completions_from_type(baseType, items, has_items);
        }

        // If not found via incomplete MemberExpr, try NodeFinder approach
        if (!has_items && dot_col >= 0) {
            int base_col = dot_col - 1;
            while (base_col >= 0 && std::isspace(doc.source[current_pos + base_col])) {
                base_col--;
            }
            if (base_col < 0) base_col = 0;

            NodeFinder base_finder(uri, line, base_col);
            for (const auto& mod : doc.resolvedAST) {
                base_finder.find_in_module(*mod);
                if (base_finder.found_decl) break;
            }
            if (base_finder.found_decl && base_finder.found_decl->type) {
                std::cerr << "[LSP] Found base decl via NodeFinder: " << base_finder.found_decl->identifier
                          << " type=" << base_finder.found_decl->type->to_str() << std::endl;
                collect_completions_from_type(base_finder.found_decl->type.get(), items, has_items);
            } else {
                std::cerr << "[LSP] NodeFinder at " << base_col << " found nothing." << std::endl;
            }
        }
    } else {
        // Non-member completion: try NodeFinder at current position (for direct type member access if any)
        NodeFinder base_finder(uri, line, col);
        for (const auto& mod : doc.resolvedAST) {
            base_finder.find_in_module(*mod);
            if (base_finder.found_decl) break;
        }
        if (base_finder.found_decl && base_finder.found_decl->type) {
            std::cerr << "[LSP] Found non-member decl via NodeFinder: " << base_finder.found_decl->identifier
                      << std::endl;
            collect_completions_from_type(base_finder.found_decl->type.get(), items, has_items);
        }
    }

    std::cerr << "[LSP] Completion has_items=" << has_items << std::endl;

    std::stringstream ss;
    ss << "{\"isIncomplete\":false,\"items\":[" << items.str() << "]}";
    send_response(id, ss.str());
}

void LSPServer::publish_diagnostics(const std::string& filename, const std::vector<SourceLocation>& errors,
                                    const std::vector<std::string>& messages) {
    std::stringstream ss;
    ss << "{\"uri\":\"file://" << filename << "\",\"diagnostics\":[";
    for (size_t i = 0; i < errors.size(); ++i) {
        const auto& e = errors[i];
        ss << "{"
           << "\"range\":{"
           << "\"start\":{\"line\":" << (e.line - 1) << ",\"character\":" << e.col << "},"
           << "\"end\":{\"line\":" << (e.line - 1) << ",\"character\":" << (e.col + 1) << "}"
           << "},"
           << "\"severity\":1,"
           << "\"message\":\"" << escape_json(messages[i]) << "\""
           << "}";
        if (i < errors.size() - 1) ss << ",";
    }
    ss << "]}";
    send_notification("textDocument/publishDiagnostics", ss.str());
}

void LSPServer::process_file(const std::string& filename, const std::string& source) {
    std::cerr << "[LSP] Processing file: " << filename << std::endl;
    Driver::instance().m_options.source = filename;
    if (m_documents.find(filename) == m_documents.end()) {
        m_documents[filename] = {source, nullptr, nullptr, {}};
    } else {
        m_documents[filename].source = source;
        m_documents[filename].resolvedAST.clear();
        m_documents[filename].sema = nullptr;
    }

    std::vector<SourceLocation> errors;
    std::vector<std::string> messages;

    // Capture cerr
    std::stringstream err_ss;
    auto old_cerr = std::cerr.rdbuf(err_ss.rdbuf());

    try {
        // (Re)initialize Driver instance with correct options BEFORE parsing
        CompilerOptions opts;
        opts.source = filename;
        if (!m_std_path.empty()) {
            opts.imports["std"] = m_std_path;
        }

        if (Driver::instance_ptr()) {
            Driver::instance().m_options = opts;
            Driver::instance().imported_modules.clear();
        } else {
            Driver::create_instance(opts);
        }

        Lexer lexer(filename, source);
        Parser parser(lexer);
        auto [ast, success] = parser.parse_source_file();

        if (ast) {
            // Resolve imports (recursively parse imported files)
            Driver::instance().import_pass(ast);

            auto sema = makePtr<Sema>(std::move(ast));
            auto resolvedTree = sema->resolve_ast_decl(false);
            if (!resolvedTree.empty()) {
                bool bodySuccess = sema->resolve_ast_body(resolvedTree);
                std::cerr << "[LSP] resolve_ast_body success=" << bodySuccess << " size=" << resolvedTree.size()
                          << std::endl;
                m_documents[filename].resolvedAST = std::move(resolvedTree);
            } else {
                std::cerr << "[LSP] resolve_ast_decl returned empty tree" << std::endl;
            }
            m_documents[filename].sema = std::move(sema);
        } else {
            std::cerr << "[LSP] Parser returned null AST" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[LSP] Exception during processing: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[LSP] Unknown exception during processing" << std::endl;
    }

    std::cerr.rdbuf(old_cerr);

    // Simple parsing of captured errors
    std::string line;
    while (std::getline(err_ss, line)) {
        if (line.empty()) continue;
        // Format: file:line:col: error: msg
        size_t first_colon = line.find(':');
        if (first_colon == std::string::npos) continue;
        size_t second_colon = line.find(':', first_colon + 1);
        if (second_colon == std::string::npos) continue;
        size_t third_colon = line.find(':', second_colon + 1);
        if (third_colon == std::string::npos) continue;

        std::string f = line.substr(0, first_colon);
        if (f != filename) continue;

        try {
            int l = std::stoi(line.substr(first_colon + 1, second_colon - first_colon - 1));
            int c = std::stoi(line.substr(second_colon + 1, third_colon - second_colon - 1)) - 1;

            size_t msg_start = line.find("error: ", third_colon);
            if (msg_start == std::string::npos) msg_start = line.find("warning: ", third_colon);
            if (msg_start == std::string::npos) continue;

            std::string msg = line.substr(msg_start);
            errors.push_back({f, (size_t)l, (size_t)c});
            messages.push_back(msg);
        } catch (...) {
            continue;
        }
    }

    publish_diagnostics(filename, errors, messages);
}

}  // namespace DMZ::lsp
