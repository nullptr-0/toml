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
- **Semantic Highlighting**  
  Colorizes TOML-specific elements include:  
  - Date-times (`2023-01-01T12:00:00Z`)
  - Booleans (`true`/`false`)
  - Identifiers (`key-name`)

- **Document diagnostics**
  Error and warning diagnostics with precise line/column positioning

- **Language Server Integration**  
  Powered by the [native TOML implementation](https://github.com/nullptr-0/toml)


### 🔧 Editor Integration
- `.toml` file association
- Bracket matching for tables/arrays
- Comment toggling (`#` syntax)
- Auto-closing for brackets and quotations

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