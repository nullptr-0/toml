# toml: A TOML Implementation

A C++20 implementation of a TOML parser and language server with JSON conversion capabilities.

## Features

- **TOML Parsing**: Validates and parses TOML files.
- **JSON Conversion**: Converts parsed TOML content into JSON format.
- **Language Server Support**: LSP integration via standard IO, socket, and named pipe.
- **Cross-Platform**: Supports Windows, Unix-like systems and NodeJS environment.
- **Testing**: Works with [toml-test](https://github.com/toml-lang/toml-test) test suite.

## Dependencies

- **C++20 Compiler** (e.g., GCC 11+, Clang 12+, MSVC 2022+)
- **CMake 3.12+**
- **Boost.Regex** (included in `lib/regex`)
- [nlohmann/json](https://github.com/nlohmann/json) (included in `lib/json`)

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/nullptr-0/toml.git
   cd toml
   ```
2. Build from source:
   - Build the native version:
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```
   - Build WebAssembly module:
   ```bash
   ./BuildWasm
   ```
   - Build the NodeJS version:
   ```bash
   cd node
   npm install
   npm run compile
   ```

## Usage

### Parse TOML to JSON
1. **Output to standard IO**
   ```bash
   path/to/toml --parse path/to/input.toml
   ```
2. **Output to file**
   ```bash
   path/to/toml --parse path/to/input.toml --output path/to/output.json
   ```
Outputs JSON and error/warning listings (and debug information if DEBUG preprocessor definition is present when building).
Throws exceptions if DEBUG preprocessor definition is present when building.

### Run Language Server
1. **Standard IO Mode**:
   ```bash
   path/to/toml --langsvr --stdio
   ```
2. **Socket Mode**:
   ```bash
   path/to/toml --langsvr --socket=8080
   ```
   or
   ```bash
   path/to/toml --langsvr --port=8080
   ```
3. **Named Pipe Mode**:
   ```bash
   path/to/toml --langsvr --pipe=PipeName
   ```

### Run Tests
1. **Input via Standard IO**:
   ```bash
   path/to/test --parse
   ```
1. **Input via File**:
   ```bash
   path/to/test --parse path/to/test.toml
   ```
Outputs JSON (and debug information and error/warning listings if DEBUG preprocessor definition is present when building).
Throws exceptions if DEBUG preprocessor definition is present when building.

## Project Structure
```
├── CMakeLists.txt
├── driver/          # Main and test driver
├── lexer/           # Lexer
├── rdparser/        # Recursive descent parser
├── langsvr/         # Language Server
├── shared/          # Common utilities
├── node/            # NodeJS wrapper
└── lib/             # External Libraries
```

## License
- **Project License**: Apache 2.0 License
- **nlohmann/json**: MIT License
- **Boost.Regex**: Boost Software License 1.0

## Contributing
1. Fork the repository
2. Create a feature branch
3. Submit a pull request with tests
4. Follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)

## Feedback
1. Search in the issues in case a similar problem has already been reported
2. Gather as much and detailed information as you can
3. Create an issue
4. Follow the [“How To Ask Questions The Smart Way” Guidelines](http://www.catb.org/~esr/faqs/smart-questions.html)
