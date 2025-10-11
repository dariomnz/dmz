import { spawn } from "child_process";
import * as vscode from "vscode";

export function activate(context: vscode.ExtensionContext) {
	const outputChannel = vscode.window.createOutputChannel("dmz Formatter");
	let disposables: readonly vscode.Disposable[] = [];

	vscode.workspace.onDidChangeConfiguration((e) => {
		if (!e.affectsConfiguration("dmz-formatter")) return;
		disposables.forEach((d) => d.dispose());
		disposables = registerFormatters(outputChannel);
	});

	disposables = registerFormatters(outputChannel);
}

const registerFormatters = (
	outputChannel: vscode.OutputChannel,
): readonly vscode.Disposable[] => {


	return [
		vscode.languages.registerDocumentFormattingEditProvider("dmz", {
			provideDocumentFormattingEdits(document, options) {
				return formatDocument(document, options, outputChannel);
			},
		}),
	];
};

const formatDocument = (
	document: vscode.TextDocument,
	options: vscode.FormattingOptions,
	outputChannel: vscode.OutputChannel,
): Promise<vscode.TextEdit[]> => {

	const command: string = "dmz -fmt -";

	const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);
	const backupFolder = vscode.workspace.workspaceFolders?.[0];
	const cwd = workspaceFolder?.uri?.fsPath || backupFolder?.uri.fsPath;

	return new Promise<vscode.TextEdit[]>(async (resolve, reject) => {
		outputChannel.appendLine(`Started formatter: ${command} on file ${document.fileName}`);

		const textToFormat = document.getText();
		const targetRange = new vscode.Range(
			document.lineAt(0).range.start,
			document.lineAt(document.lineCount - 1).rangeIncludingLineBreak.end,
		);

		const process = spawn(command, { cwd, shell: true });
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
				if (stderr !== "") outputChannel.appendLine(`Stderr:\n${stderr}`);
				if (stdout !== "") outputChannel.appendLine(`Stdout:\n${stdout}`);
				vscode.window.showErrorMessage(message, "Show output").then((selection) => {
					if (selection === "Show output") outputChannel.show();
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
export function deactivate() { }