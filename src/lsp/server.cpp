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
                  "\"completionProvider\":null"
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
        const auto& loc = finder.found_decl->location;
        std::stringstream ss;
        ss << "{\"uri\":\"file://" << loc.file_name << "\",\"range\":{"
           << "\"start\":{\"line\":" << (loc.line - 1) << ",\"character\":" << loc.col << "},"
           << "\"end\":{\"line\":" << (loc.line - 1)
           << ",\"character\":" << (loc.col + finder.found_decl->identifier.length()) << "}"
           << "}}";
        send_response(id, ss.str());
    } else {
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

void LSPServer::on_completion(const std::string& id, const std::string& params) {
    (void)params;
    send_response(id, "{\"isIncomplete\":false,\"items\":[]}");
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
                sema->resolve_ast_body(resolvedTree);
                m_documents[filename].resolvedAST = std::move(resolvedTree);
            }
            m_documents[filename].sema = std::move(sema);
        }
    } catch (...) {
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
