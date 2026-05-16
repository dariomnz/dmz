#include "lsp/server.hpp"

#include <fstream>
#include <iostream>

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
        std::cerr << "[LSP] Received: " << line << std::endl;
        if (line.starts_with("Content-Length: ")) {
            int length = std::stoi(line.substr(16));

            while (std::getline(std::cin, line) && !line.empty() && line != "\r") {
            }

            std::string body(length, ' ');
            std::cin.read(&body[0], length);
            std::cerr << "[LSP] Received body: " << body << std::endl;
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
    } else if (method == "textDocument/didClose") {
        on_did_close(message);
    } else if (method == "textDocument/definition") {
        on_definition(id, message);
    } else if (method == "textDocument/hover") {
        on_hover(id, message);
    } else if (method == "textDocument/semanticTokens/full") {
        on_semantic_tokens(id, message);
    } else if (method == "textDocument/semanticTokens/range") {
        on_semantic_tokens_range(id, message);
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

    send_response(
        id,
        "{\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"definitionProvider\":true,"
        "\"hoverProvider\":true,"
        "\"semanticTokensProvider\":{"
        "\"legend\":{"
        "\"tokenTypes\":[\"type\",\"function\",\"parameter\",\"variable\",\"property\",\"namespace\",\"number\"],"
        "\"tokenModifiers\":[\"declaration\"]"
        "},"
        "\"full\":true,"
        "\"range\":true"
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
        std::cerr << "[LSP] Processing file with text:" << std::endl;
        // std::cerr << text << std::endl;
        process_file(path, text);
        return;
    }

    std::cerr << "[LSP] Processing file from disk" << std::endl;
    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        process_file(path, ss.str());
    }
}

void LSPServer::on_did_change(const std::string& params) { on_did_open(params); }

void LSPServer::on_did_close(const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    invalidate_module(uri);

    m_documents.erase(uri);
    if (m_documents.empty()) {
        m_module_cache.clear();
    } else if (m_module_cache.count(uri)) {
        m_module_cache[uri].source = "";
    }

    dump_documents();
    dump_cache();
}

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
    if (!doc.mainModule) {
        std::cerr << "[LSP] Module for " << uri << " is null, returning null definition" << std::endl;
        send_response(id, "null");
        return;
    }

    size_t line = std::stoul(line_str) + 1;
    size_t col = std::stoul(char_str);

    NodeFinder finder(uri, line, col);
    if (doc.mainModule) {
        finder.find_in_module(*doc.mainModule);
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
    if (doc.mainModule) {
        finder.find_in_module(*doc.mainModule);
    }

    if (finder.found_decl) {
        std::stringstream ss;
        ss << "{\"contents\":{\"kind\":\"markdown\",\"value\":\"```dmz\\n"
           << escape_json(finder.found_decl->name()) << ": ";
        if (auto funcType = dynamic_cast<ResolvedTypeFunction*>(finder.found_decl->type.get())) {
            ss << escape_json(funcType->to_str_with_params());
        } else {
            ss << escape_json(finder.found_decl->type->to_str());
        }
        ConstantExpressionEvaluator cee;
        if (auto val = cee.evaluate_decl(*finder.found_decl)) {
            ss << " = " << *val;
        }
        ss << "\\n```\"}}";
        std::cerr << "[LSP] Sending hover response: " << ss.str() << std::endl;
        send_response(id, ss.str());
        return;
    } else if (finder.found_expr) {
        if (auto comptimeVal = finder.found_expr->get_constant_value()) {
            std::stringstream ss;
            ss << "{\"contents\":{\"kind\":\"markdown\",\"value\":\"```dmz\\n";
            ss << escape_json(comptimeVal->to_str()) << "\\n```\"}}";
            std::cerr << "[LSP] Sending hover response: " << ss.str() << std::endl;
            send_response(id, ss.str());
            return;
        }
        std::cerr << "[LSP] Found expression, but could not evaluate to constant value" << std::endl;
    }
    send_response(id, "null");
}

void LSPServer::on_semantic_tokens(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    if (m_module_cache.find(uri) == m_module_cache.end()) {
        std::cerr << "[LSP] Document " << uri << " not found for semantic tokens" << std::endl;
        dump_documents();
        dump_cache();
        send_response(id, "{\"data\":[]}");
        return;
    }

    auto& cache_entry = m_module_cache[uri];
    if (!cache_entry.module) {
        if (!cache_entry.source.empty()) {
            process_file(uri, cache_entry.source);
        }
        if (!cache_entry.module) {
            std::cerr << "[LSP] No main module for " << uri << std::endl;
            dump_documents();
            dump_cache();
            send_response(id, "{\"data\":[]}");
            return;
        }
    }

    SemanticTokensCollector collector(uri, cache_entry.source);
    std::vector<SemanticToken> tokens = collector.collect(cache_entry.module.get());

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
    std::cerr << "[LSP] Sending semantic tokens response" << std::endl;
    send_response(id, ss.str());
}

void LSPServer::on_semantic_tokens_range(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    if (m_module_cache.find(uri) == m_module_cache.end()) {
        send_response(id, "{\"data\":[]}");
        return;
    }

    auto& cache_entry = m_module_cache[uri];
    if (!cache_entry.module) {
        if (!cache_entry.source.empty()) {
            process_file(uri, cache_entry.source);
        }
        if (!cache_entry.module) {
            std::cerr << "[LSP] No main module for " << uri << std::endl;
            dump_documents();
            dump_cache();
            send_response(id, "{\"data\":[]}");
            return;
        }
    }

    // Very basic parsing of range from JSON
    size_t startLine = 0;
    size_t endLine = 0;

    size_t rangePos = params.find("\"range\"");
    if (rangePos != std::string::npos) {
        size_t startPos = params.find("\"start\"", rangePos);
        if (startPos != std::string::npos) {
            std::string lineVal = get_json_value(params.substr(startPos), "line");
            if (!lineVal.empty()) startLine = std::stoul(lineVal);
        }
        size_t endPos = params.find("\"end\"", rangePos);
        if (endPos != std::string::npos) {
            std::string lineVal = get_json_value(params.substr(endPos), "line");
            if (!lineVal.empty()) endLine = std::stoul(lineVal);
        }
    }

    SemanticTokensCollector collector(uri, cache_entry.source);
    std::vector<SemanticToken> all_tokens = collector.collect(cache_entry.module.get());

    std::vector<SemanticToken> tokens;
    for (const auto& t : all_tokens) {
        if (t.line >= startLine && t.line <= endLine) {
            tokens.push_back(t);
        }
    }

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

void LSPServer::on_completion(const std::string& id, const std::string& params) {
    std::string uri = get_json_value(params, "uri");
    if (uri.starts_with("file://")) uri = uri.substr(7);

    std::string line_str = get_json_value(params, "line");
    std::string char_str = get_json_value(params, "character");

    if (line_str.empty() || char_str.empty() || m_module_cache.find(uri) == m_module_cache.end()) {
        std::cerr << "[LSP] Completion request failed: invalid parameters." << std::endl;
        send_response(id, "{\"isIncomplete\":false,\"items\":[]}");
        return;
    }

    auto& cache_entry = m_module_cache[uri];
    size_t line = std::stoul(line_str) + 1;
    size_t col = std::stoul(char_str);

    std::cerr << "[LSP] Completion at line=" << line << " col=" << col << std::endl;

    // Scan backwards from cursor to find if we're in a member completion context
    size_t current_line = 1;
    size_t current_pos = 0;
    while (current_line < line && current_pos < cache_entry.source.length()) {
        if (cache_entry.source[current_pos] == '\n') current_line++;
        current_pos++;
    }

    bool is_member_completion = false;
    int dot_col = -1;
    int col_iter = (int)col;
    while (col_iter >= 0 && current_pos + col_iter < cache_entry.source.length()) {
        char c = cache_entry.source[current_pos + col_iter];
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
        if (cache_entry.module) {
            const ResolvedType* baseType = find_incomplete_member_base_type(cache_entry.module.get(), uri, line);
            if (baseType) {
                std::cerr << "[LSP] Found base type for member completion: " << baseType->to_str() << std::endl;
                collect_completions_from_type(baseType, items, has_items);
            }

            // If not found via incomplete MemberExpr, try NodeFinder approach
            if (!has_items && dot_col >= 0) {
                int base_col = dot_col - 1;
                while (base_col >= 0 && std::isspace(cache_entry.source[current_pos + base_col])) {
                    base_col--;
                }
                if (base_col < 0) base_col = 0;

                NodeFinder base_finder(uri, line, (size_t)base_col);
                base_finder.find_in_module(*cache_entry.module);

                if (base_finder.found_decl && base_finder.found_decl->type) {
                    std::cerr << "[LSP] Found base decl via NodeFinder: " << base_finder.found_decl->identifier
                              << " type=" << base_finder.found_decl->type->to_str() << std::endl;
                    collect_completions_from_type(base_finder.found_decl->type.get(), items, has_items);
                } else if (base_finder.found_expr && base_finder.found_expr->type) {
                    std::cerr << "[LSP] Found base expr via NodeFinder type=" << base_finder.found_expr->type->to_str()
                              << std::endl;
                    collect_completions_from_type(base_finder.found_expr->type.get(), items, has_items);
                } else {
                    std::cerr << "[LSP] NodeFinder at " << base_col << " found nothing." << std::endl;
                }
            }
        }
    } else {
        // Non-member completion: try NodeFinder at current position (for direct type member access if any)
        if (cache_entry.module) {
            NodeFinder base_finder(uri, line, col);
            base_finder.find_in_module(*cache_entry.module);
            if (base_finder.found_decl && base_finder.found_decl->type) {
                std::cerr << "[LSP] Found non-member decl via NodeFinder: " << base_finder.found_decl->identifier
                          << std::endl;
                collect_completions_from_type(base_finder.found_decl->type.get(), items, has_items);
            }
        }
    }

    if (!has_items && cache_entry.module) {
        const ResolvedScope* scope = find_scope_at_position(cache_entry.module.get(), uri, line, col);
        if (scope) {
            std::cerr << "[LSP] Found scope at position: " << line << ":" << col << std::endl;
            collect_scope_completions(scope, line, items, has_items);
        } else if (cache_entry.module->scope) {
            std::cerr << "[LSP] Using module scope" << std::endl;
            collect_scope_completions(cache_entry.module->scope.get(), line, items, has_items);
        }
    }

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
    // 1. Check for disk changes in modules not open in editor
    std::vector<std::string> to_invalidate;
    for (auto& [path, entry] : m_module_cache) {
        if (!m_documents.count(path)) {
            if (std::filesystem::exists(path)) {
                auto current_time = std::filesystem::last_write_time(path);
                if (current_time > entry.last_write_time) {
                    std::cerr << "[LSP] Module changed on disk: " << path << std::endl;
                    to_invalidate.push_back(path);
                }
            } else {
                std::cerr << "[LSP] Module deleted: " << path << std::endl;
                to_invalidate.push_back(path);
            }
        }
    }
    for (auto const& p : to_invalidate) invalidate_module(p);

    // 2. Check if main file needs invalidation (source changed)
    if (m_module_cache.count(filename) && m_module_cache[filename].source != source) {
        std::cerr << "[LSP] Main file changed: " << filename << std::endl;
        invalidate_module(filename);
    }

    // 3. If already cached and valid, just return
    if (m_module_cache.count(filename) && m_module_cache[filename].module) {
        m_documents[filename].mainModule = m_module_cache[filename].module.get();
        return;
    }

    CompilerOptions options;
    options.source = filename;
    options.isModule = true;
    Driver driver(options);

    m_documents[filename].mainModule = nullptr;

    std::vector<SourceLocation> errors;
    std::vector<std::string> messages;

    // Capture cerr
    std::stringstream err_ss;
    auto old_cerr = std::cerr.rdbuf(err_ss.rdbuf());

    try {
        Lexer lexer(filename, source);
        Parser parser(driver, lexer);
        auto [ast, success] = parser.parse_source_file();

        if (ast) {
            Sema sema(driver, std::move(ast));

            // Move all modules from global cache to sema
            for (auto& [path, entry] : m_module_cache) {
                if (entry.module) {
                    sema.add_pre_resolved_module(std::move(entry.module));
                }
            }

            auto resolvedTree = sema.resolve_ast_decl(filename, false);
            if (!resolvedTree.empty()) {
                sema.resolve_ast_body(resolvedTree);

                vec<ResolvedModuleDecl*> toResolve;
                // Collect modules to ensure resolved
                for (auto&& mod : resolvedTree) {
                    if (m_documents.count(mod->module_path)) {
                        toResolve.emplace_back(mod.get());
                    }
                }

                // Move all modules back into sema so ensure_fully_resolved can find them
                for (auto& mod : resolvedTree) {
                    if (mod) sema.add_pre_resolved_module(std::move(mod));
                }
                resolvedTree.clear();

                // Eager resolve all open documents
                for (auto const& mod : toResolve) {
                    sema.ensure_fully_resolved(*mod);
                }

                // Take them all back from sema to update the cache
                auto finalTree = sema.take_resolved_modules();

                // Update global cache with newly resolved modules
                for (auto&& mod : finalTree) {
                    if (mod) {
                        auto& path = mod->module_path;
                        auto& entry = m_module_cache[path];
                        entry.module = std::move(mod);
                        if (std::filesystem::exists(path)) {
                            entry.last_write_time = std::filesystem::last_write_time(path);
                        }
                        if (m_documents.count(path)) {
                            m_documents[path].mainModule = entry.module.get();
                        }

                        if (path == filename) {
                            entry.source = source;
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[LSP] Exception during processing: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[LSP] Unknown exception during processing" << std::endl;
    }

    std::cerr.rdbuf(old_cerr);

    // Legacy parsing of captured errors
    std::string line;
    while (std::getline(err_ss, line)) {
        if (line.empty()) continue;
        if (line.find(":") != std::string::npos) {
            auto first_colon = line.find(":");
            auto second_colon = line.find(":", first_colon + 1);
            auto third_colon = line.find(":", second_colon + 1);
            if (first_colon != std::string::npos && second_colon != std::string::npos &&
                third_colon != std::string::npos) {
                std::string file = line.substr(0, first_colon);
                if (file != filename) continue;
                try {
                    int l = std::stoi(line.substr(first_colon + 1, second_colon - first_colon - 1));
                    int c = std::stoi(line.substr(second_colon + 1, third_colon - second_colon - 1)) - 1;

                    size_t msg_start = line.find("error: ", third_colon);
                    if (msg_start == std::string::npos) msg_start = line.find("warning: ", third_colon);
                    if (msg_start == std::string::npos) continue;

                    std::string msg = line.substr(msg_start);
                    errors.push_back({file, (size_t)l, (size_t)c});
                    messages.push_back(msg);
                } catch (...) {
                    continue;
                }
            }
        }
    }

    m_documents[filename].errors = errors;
    m_documents[filename].messages = messages;
    publish_diagnostics(filename, errors, messages);

    dump_documents();
    dump_cache();
}

void LSPServer::dump_cache() {
    std::cerr << "[LSP] Cache dump:" << std::endl;
    for (auto const& [path, entry] : m_module_cache) {
        std::cerr << "  " << path << " " << entry.module.get() << std::endl;
    }
}

void LSPServer::dump_documents() {
    std::cerr << "[LSP] Documents dump:" << std::endl;
    for (auto const& [path, entry] : m_documents) {
        std::cerr << "  " << path << " " << entry.mainModule << std::endl;
    }
}

void LSPServer::invalidate_module(const std::string& path) {
    std::unordered_set<std::string> invalidated;
    invalidate_module(path, invalidated);

    for (auto& path : invalidated) {
        if (m_module_cache.count(path)) {
            if (m_module_cache[path].source.empty()) {
                m_module_cache.erase(path);
            } else {
                m_module_cache[path].module = nullptr;
            }
        }
        if (m_documents.count(path)) {
            m_documents[path].mainModule = nullptr;
        }
    }
}

void LSPServer::invalidate_module(const std::string& path, std::unordered_set<std::string>& invalidated) {
    if (invalidated.count(path)) return;
    invalidated.insert(path);

    if (m_module_cache.count(path) && m_module_cache[path].module) {
        std::cerr << "[LSP] Invalidating cache for: " << path << std::endl;
        for (auto it = m_module_cache[path].module->isUsedBy.begin();
             it != m_module_cache[path].module->isUsedBy.end();) {
            auto& user = *it;
            std::cerr << "[LSP] Invalidating dependent: " << user->module_path << std::endl;
            invalidate_module(user->module_path, invalidated);
            std::erase(user->dependsOn, m_module_cache[path].module.get());
            it = m_module_cache[path].module->isUsedBy.erase(it);
        }
    }
}
}  // namespace DMZ::lsp
