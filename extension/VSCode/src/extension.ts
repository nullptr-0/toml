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

  const getAndSendNewCslSchemas = (onStartUp: boolean) => {
    const config = vscode.workspace.getConfiguration();
    const cslSchemas = config.get<string>("toml.csl.cslSchemas");
    if (cslSchemas && (!onStartUp || cslSchemas.length)) {
      const cslSchema = config.get<string>("toml.csl.cslSchema");
      client.sendRequest("configSchemaLanguage/setSchemas", { schemas: cslSchemas, schema: cslSchema ? cslSchema : "" });
    }
  };

  const getAndSendNewCslSchema = (onStartUp: boolean) => {
    const config = vscode.workspace.getConfiguration();
    const cslSchema = config.get<string>("toml.csl.cslSchema");
    if (cslSchema && (!onStartUp || cslSchema.length)) {
      client.sendRequest("configSchemaLanguage/setSchema", { schema: cslSchema });
    }
  };

  context.subscriptions.push(vscode.workspace.onDidChangeConfiguration(e => {
    if (e.affectsConfiguration('toml.csl.cslSchemas')) {
      getAndSendNewCslSchemas(false);
    }
    if (e.affectsConfiguration('toml.csl.cslSchema')) {
      getAndSendNewCslSchema(false);
    }
  }));

  client
    .start()
    .then(() => {
      vscode.window.showInformationMessage("TOML Language Service Ready");
      getAndSendNewCslSchemas(true);
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
