cmake_minimum_required(VERSION 3.12)

project(toml
    DESCRIPTION "A TOML Implementation")

set(CMAKE_CXX_STANDARD 20)

option(BUILD_WASM "Build for WebAssembly using Emscripten" OFF)

if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    if (BUILD_WASM)
        add_compile_definitions(STDIO_ONLY)
    else()
        add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS)
    endif()
endif()

include_directories(lib/json)

add_subdirectory(lib/regex)
include_directories(lib/regex/include)

if (BUILD_WASM)
    include_directories(
        $<TARGET_PROPERTY:INTERFACE_INCLUDE_DIRECTORIES>
    )
    
    set(RUNTIME_METHODS_LIST
        FS
        callMain
    )
    list(JOIN RUNTIME_METHODS_LIST "," RUNTIME_METHODS)
    
    set(EMSCRIPTEN_FLAGS
        "-s NO_DYNAMIC_EXECUTION=1"
        "-s MODULARIZE=1"
        "-s EXIT_RUNTIME=1"
        "-s FORCE_FILESYSTEM=1"
        "-s EXPORT_NAME=TomlModule"
        "-s EXPORTED_RUNTIME_METHODS=${RUNTIME_METHODS}"
        "-lnodefs.js"
    )
    set(EMSCRIPTEN_LEGACY_FLAGS
        ${EMSCRIPTEN_FLAGS}
        "-s WASM=0"
    )
    set(EMSCRIPTEN_WASM_BUNDLE_FLAGS
        ${EMSCRIPTEN_FLAGS}
        "-s SINGLE_FILE=1"
    )
    
    list(JOIN EMSCRIPTEN_FLAGS             " " EMSCRIPTEN_FLAGS_STR)
    list(JOIN EMSCRIPTEN_LEGACY_FLAGS      " " EMSCRIPTEN_LEGACY_FLAGS_STR)
    list(JOIN EMSCRIPTEN_WASM_BUNDLE_FLAGS " " EMSCRIPTEN_WASM_BUNDLE_FLAGS_STR)
endif()

set(TOML_SOURCES
    shared/Shared.cpp
    lexer/TomlLexer.cpp
    rdparser/TomlRdParser.cpp
    langsvr/TomlLangSvr.cpp
    driver/Toml.cpp
)

set(TEST_SOURCES
    shared/Shared.cpp
    lexer/TomlLexer.cpp
    rdparser/TomlRdParser.cpp
    driver/Test.cpp
)

if (BUILD_WASM)
    add_executable(TomlWasm ${TOML_SOURCES})
    target_link_libraries(TomlWasm boost_regex)
    set_target_properties(
        TomlWasm
        PROPERTIES LINK_FLAGS ${EMSCRIPTEN_FLAGS_STR}
    )

    add_executable(TomlWasmBundle ${TOML_SOURCES})
    target_link_libraries(TomlWasmBundle boost_regex)
    set_target_properties(
        TomlWasmBundle
        PROPERTIES LINK_FLAGS ${EMSCRIPTEN_WASM_BUNDLE_FLAGS_STR}
    )

    add_executable(TomlLegacy ${TOML_SOURCES})
    target_link_libraries(TomlLegacy boost_regex)
    set_target_properties(
        TomlLegacy
        PROPERTIES LINK_FLAGS ${EMSCRIPTEN_LEGACY_FLAGS_STR}
    )
else()
    add_executable(toml ${TOML_SOURCES})
    target_link_libraries(toml boost_regex)

    add_executable(test ${TEST_SOURCES})
    target_link_libraries(test boost_regex)
endif()
