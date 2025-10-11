"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const child_process_1 = require("child_process");
const vscode = __importStar(require("vscode"));
function activate(context) {
    const outputChannel = vscode.window.createOutputChannel("dmz Formatter");
    let disposables = [];
    vscode.workspace.onDidChangeConfiguration((e) => {
        if (!e.affectsConfiguration("dmz-formatter"))
            return;
        disposables.forEach((d) => d.dispose());
        disposables = registerFormatters(outputChannel);
    });
    disposables = registerFormatters(outputChannel);
}
const registerFormatters = (outputChannel) => {
    return [
        vscode.languages.registerDocumentFormattingEditProvider("dmz", {
            provideDocumentFormattingEdits(document, options) {
                return formatDocument(document, options, outputChannel);
            },
        }),
    ];
};
const formatDocument = (document, options, outputChannel) => {
    const command = "dmz -fmt -";
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);
    const backupFolder = vscode.workspace.workspaceFolders?.[0];
    const cwd = workspaceFolder?.uri?.fsPath || backupFolder?.uri.fsPath;
    return new Promise(async (resolve, reject) => {
        outputChannel.appendLine(`Started formatter: ${command} on file ${document.fileName}`);
        const textToFormat = document.getText();
        const targetRange = new vscode.Range(document.lineAt(0).range.start, document.lineAt(document.lineCount - 1).rangeIncludingLineBreak.end);
        const process = (0, child_process_1.spawn)(command, { cwd, shell: true });
        process.stdout.setEncoding("utf8");
        process.stderr.setEncoding("utf8");
        let stdout = "";
        let stderr = "";
        process.stdout.on("data", (chunk) => {
            stdout += chunk;
        });
        process.stderr.on("data", (chunk) => {
            stderr += chunk;
        });
        process.on("close", (code, signal) => {
            if (code !== 0) {
                const reason = signal
                    ? `terminated by signal ${signal} (likely due to a timeout or external termination)`
                    : `exited with code ${code}`;
                const message = `Formatter failed: ${command} on file ${document.fileName}\nReason: ${reason}`;
                outputChannel.appendLine(message);
                if (stderr !== "")
                    outputChannel.appendLine(`Stderr:\n${stderr}`);
                if (stdout !== "")
                    outputChannel.appendLine(`Stdout:\n${stdout}`);
                vscode.window.showErrorMessage(message, "Show output").then((selection) => {
                    if (selection === "Show output")
                        outputChannel.show();
                });
                reject(new Error(message));
                return;
            }
            if (textToFormat.length > 0 && stdout.length === 0) {
                outputChannel.appendLine(`Formatter returned nothing - not applying changes.`);
                resolve([]);
                return;
            }
            outputChannel.appendLine(`Finished running formatter: ${command} on file ${document.fileName}`);
            if (stderr.length > 0)
                outputChannel.appendLine(`Possible issues occurred:\n${stderr}`);
            resolve([new vscode.TextEdit(targetRange, stdout)]);
            return;
        });
        process.stdin.write(textToFormat);
        process.stdin.end();
    });
};
// this method is called when your extension is deactivated
function deactivate() { }
//# sourceMappingURL=extension.js.map