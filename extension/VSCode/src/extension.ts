import * as vscode from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
} from "vscode-languageclient/node";

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
  const serverOptions: ServerOptions = {
    // module: context.asAbsolutePath("../../node/out/toml.js"),
    // options: { cwd: context.asAbsolutePath("../../node/out") },
    command: context.extensionPath + "/bin/toml",
    args: ["--langsvr"],
    // transport: TransportKind.pipe,
    // transport: {
    //   kind: TransportKind.socket,
    //   port: 2087,
    // },
    transport: TransportKind.stdio,
  };

  // Client options
  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "toml" }],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.toml"),
    },
  };

  client = new LanguageClient(
    "TomlLanguageService",
    "TOML Language Service",
    serverOptions,
    clientOptions
  );

  client
    .start()
    .then(() => {
      vscode.window.showInformationMessage("TOML Language Service Ready");
    })
    .catch((reason) => {
      console.error("Cannot start toml language service: ", reason);
    });
  return;
}

export function deactivate() {
  if (!client) {
    return;
  }
  client
    .stop()
    .catch((reason) => {
      console.error("Cannot stop toml language service: ", reason);
    });
  return;
}
