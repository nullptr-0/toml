# TOML Language Service for Visual Studio Code

![Version](https://img.shields.io/badge/version-0.0.1-blue)
[![VS Code Version](https://img.shields.io/badge/vscode-%3E%3D1.98.0-blueviolet)](https://code.visualstudio.com/)
[![Built with TypeScript](https://img.shields.io/badge/Built%20With-TypeScript-3178c6)](https://www.typescriptlang.org/)
[![Powered by LSP](https://img.shields.io/badge/Powered%20By-Language%20Server%20Protocol-3c8dbc)](https://microsoft.github.io/language-server-protocol/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green)](https://www.apache.org/licenses/LICENSE-2.0.html)

A VS Code extension providing TOML language support. Built on top of a native TOML implementation.

![TOML Semantic Highlighting Demo](https://raw.githubusercontent.com/nullptr-0/toml/refs/heads/master/extension/VSCode/img/demo.png)

## Features

### ✨ Core Capabilities

#### 🖍️ Semantic Highlighting

* Colorizes TOML-specific syntax elements:

  * **Date-times** (`2023-01-01T12:00:00Z`)
  * **Booleans** (`true` / `false`)
  * **Numbers** (`0xff`, `1.5e2`)
  * **Identifiers** (`keyName`)
  * **Strings** (`"a string"`)
  * **Punctuators**
  * **Operators**
  * **Comments** (`# a comment`)

#### 🛠️ Document Diagnostics

* Reports **errors and warnings** with exact **line/column positioning**
* CSL (Config Schema Language) validation support - reports schema violations in diagnostics

#### 🔍 Hover Information

* Provides **hover cards** for tables and arrays with:

  * Type info (`Table` / `Array`)
  * Entry count
  * Mutability
  * Defined location (line & column)

#### 🔧 Formatting

* Formats TOML files based on parsed structure

#### 📚 Go to Definition

* Navigates to the definition of a key (table/array)

#### 🔁 Renaming

* Renames **all references** to a key
* Uses token-to-structure mapping to rename safely and accurately

#### 🔗 Find References

* Finds **all references** to a key in a document

#### 🔄 Ranges Folding

* Supports folding for:

  * **Tables and arrays**
  * **Multi-line comments**

#### 🔡 Completion Suggestions

* Auto-suggests:

  * Known keys in scope
  * Schema-defined keys (optional/mandatory)
* Provides **context-aware completions** including:

  * Dot-navigation (`table.key`)
  * Schema-aware completions with fallback
  * Details and definitions for each completion

#### 🧠 Schema Integration (CSL)

* Set of **config schemas** can be provided
* Dynamically validates and completes keys against selected schema
* Supports switching active schema on the fly

#### 🧩 Language Server Integration
* Powered by the [native TOML implementation](https://github.com/nullptr-0/toml)


### 🧰 Editor Integration
* `.toml` file association
* Bracket matching for tables/arrays
* Comment toggling (`#` syntax)
* Auto-closing for brackets and quotations

## Installation

### Manual Installation
```bash
git clone https://github.com/nullptr-0/toml.git
cd toml/extension/VSCode
npm install
vsce package
code --install-extension toml-0.0.1.vsix
```

## Configuration

### Semantic Token Mapping
Customize colors via `settings.json`:
```json
{
  "editor.semanticTokenColorCustomizations": {
    "[Your Theme]": {
      "rules": {
        "datetime": {"foreground": "#2ecc71"},
        "boolean": {"foreground": "#e74c3c", "fontStyle": "bold"}
      }
    }
  }
}
```
Valid semantic tokens are: `datetime`, `number`, `boolean`, `identifier`, `punctuator`, `operator`, `comment`, `string`, and `unknown`

## Development

### Prerequisites
- Node.js 20.x (older versions may also work but not recommended)
- VS Code 1.98+
- Native TOML Backend ([see core implementation](https://github.com/nullptr-0/toml))

### Build Process
```bash
# Install dependencies
npm install

# Development mode (watch files)
npm run watch

# Production build
npm run package

# Linting
npm run lint
```

### Architecture
```
├── bin/
│   └── ...                        # TOML binary and/or module
├── img/
│   ├── demo.png                   # TOML Semantic Highlighting Demo
│   └── toml.png                   # TOML icon
├── src/
│   └── extension.ts               # Extension entry point
├── .gitignore
├── .vscodeignore
├── esbuild.js
├── eslint.config.mjs
├── language-configuration.json
├── LICENSE.txt
├── package.json
├── README.md
└── tsconfig.json
```

## License
- **Extension**: [Apache 2.0 License](https://www.apache.org/licenses/LICENSE-2.0.html)
- **Dependencies**:
  - `vscode-languageclient`: MIT License
  - `nlohmann/json`: MIT License
  - `boost-regex`: Boost License

## Report Issues
[GitHub Issues](https://github.com/nullptr-0/toml/issues)