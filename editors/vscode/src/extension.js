"use strict";

const { spawn } = require("node:child_process");
const vscode = require("vscode");

const { JsonRpcClient } = require("./jsonRpcClient");

let childProcess;
let client;
let outputChannel;
let startupPromise;

function activeWorkspace() {
  const folders = vscode.workspace.workspaceFolders;
  if (!folders || folders.length === 0) {
    throw new Error("Open a workspace folder before using Sherpa.");
  }
  return folders[0];
}

async function createServer() {
  if (client) {
    return client;
  }

  const workspace = activeWorkspace();
  const configuration = vscode.workspace.getConfiguration("sherpa", workspace.uri);
  const serverPath = configuration.get("serverPath", "sherpa-server");
  const databasePath = configuration.get("databasePath", "");
  outputChannel.appendLine(`Starting ${serverPath} for ${workspace.uri.fsPath}`);

  const process = spawn(serverPath, [], {
    stdio: ["pipe", "pipe", "pipe"],
    windowsHide: true,
  });
  const rpc = new JsonRpcClient(process);
  childProcess = process;
  client = rpc;
  rpc.on("stderr", (message) => outputChannel.append(message));
  rpc.on("exit", () => {
    if (client === rpc) {
      client = undefined;
      childProcess = undefined;
    }
  });

  const initialization = rpc.sendRequest("initialize", {
    repository_path: workspace.uri.fsPath,
    ...(databasePath ? { database_path: databasePath } : {}),
  });
  try {
    const result = await initialization.promise;
    if (result.protocol_version !== 1) {
      throw new Error(`Unsupported Sherpa protocol version ${result.protocol_version}`);
    }
    outputChannel.appendLine(`Connected to sherpa-server ${result.server_version}`);
    return rpc;
  } catch (error) {
    rpc.dispose();
    process.kill();
    if (client === rpc) {
      client = undefined;
      childProcess = undefined;
    }
    throw error;
  }
}

async function startServer() {
  if (!startupPromise) {
    startupPromise = createServer().finally(() => {
      startupPromise = undefined;
    });
  }
  return startupPromise;
}

async function indexWorkspace() {
  const rpc = await startServer();
  await vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Notification,
      title: "Sherpa: Indexing workspace",
      cancellable: true,
    },
    async (_progress, token) => {
      const request = rpc.sendRequest("workspace/index");
      const cancellation = token.onCancellationRequested(() => request.cancel());
      try {
        const result = await request.promise;
        outputChannel.appendLine(JSON.stringify(result, null, 2));
        await vscode.window.showInformationMessage(
          `Sherpa indexed ${result.indexed_files} files in ${result.total_milliseconds} ms.`,
        );
      } catch (error) {
        if (error.code === -32800) {
          await vscode.window.showInformationMessage("Sherpa indexing request cancelled.");
          return;
        }
        throw error;
      } finally {
        cancellation.dispose();
      }
    },
  );
}

async function showStatus() {
  const rpc = await startServer();
  const result = await rpc.sendRequest("workspace/status").promise;
  outputChannel.appendLine(JSON.stringify(result, null, 2));
  outputChannel.show(true);
}

async function querySymbol() {
  const name = await vscode.window.showInputBox({
    title: "Sherpa: Query Symbol",
    prompt: "Qualified or unqualified symbol name",
    ignoreFocusOut: true,
  });
  if (!name) {
    return;
  }

  const rpc = await startServer();
  const result = await rpc.sendRequest("query/symbol", { name }).promise;
  const document = await vscode.workspace.openTextDocument({
    language: "json",
    content: `${JSON.stringify(result, null, 2)}\n`,
  });
  await vscode.window.showTextDocument(document, { preview: true });
}

function reportFailure(error) {
  outputChannel.appendLine(error.stack || String(error));
  outputChannel.show(true);
  void vscode.window.showErrorMessage(`Sherpa: ${error.message || error}`);
}

function registerCommand(context, name, callback) {
  context.subscriptions.push(
    vscode.commands.registerCommand(name, async () => {
      try {
        await callback();
      } catch (error) {
        reportFailure(error);
      }
    }),
  );
}

function activate(context) {
  outputChannel = vscode.window.createOutputChannel("Sherpa");
  context.subscriptions.push(outputChannel);
  registerCommand(context, "sherpa.indexWorkspace", indexWorkspace);
  registerCommand(context, "sherpa.showStatus", showStatus);
  registerCommand(context, "sherpa.querySymbol", querySymbol);
}

async function deactivate() {
  if (!client) {
    return;
  }

  const rpc = client;
  const process = childProcess;
  try {
    await rpc.sendRequest("shutdown").promise;
    rpc.notify("exit");
    process.stdin.end();
    await Promise.race([
      new Promise((resolve) => process.once("exit", resolve)),
      new Promise((resolve) => setTimeout(resolve, 500)),
    ]);
  } catch (error) {
    outputChannel?.appendLine(`Shutdown failed: ${error.message || error}`);
  } finally {
    rpc.dispose();
    if (process && process.exitCode === null) {
      process.kill();
    }
    client = undefined;
    childProcess = undefined;
  }
}

module.exports = {
  activate,
  deactivate,
};
