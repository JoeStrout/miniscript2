#!/bin/bash
# MS2Proto3 build orchestration script

set -e  # Exit on any error

PROJECT_ROOT="$(dirname "$0")/.."
cd "$PROJECT_ROOT"

echo "=== MS2Proto3 Build Script ==="
echo "Project root: $(pwd)"

# Parse command line arguments
TARGET="${1:-all}"
GOTO_MODE="${2:-auto}"  # auto, on, off

case "$TARGET" in
    "setup")
        echo "Setting up development environment..."
        mkdir -p build/{cs,cpp,temp}
        mkdir -p generated
        echo "Setup complete."
        ;;

    "parser")
        echo "Generating parser from grammar..."
        mkdir -p cs/Parser

        # Generate parser (using hand-written lexer in cs/Lexer.cs)
        echo "Running GPPG on grammar/miniscript.y..."
        dotnet tools/gppg/Gppg.dll /out:cs/Parser/MiniScriptParser.cs grammar/miniscript.y
        if [ $? -ne 0 ]; then
            echo "Error: Failed to generate parser"
            exit 1
        fi

        # Post-process: Replace EOF with END_OF_FILE for consistency with C++
        echo "Post-processing: Renaming EOF token to END_OF_FILE..."
        sed -i '' -e 's/,EOF=/,END_OF_FILE=/g' -e 's/\.EOF)/\.END_OF_FILE)/g' cs/Parser/MiniScriptParser.cs

        echo "Parser generation complete."
        echo "Generated file: cs/Parser/MiniScriptParser.cs"
        echo "Using hand-written lexer: cs/Lexer.cs"
        ;;

    "cs")
        echo "Building C# version..."
        cd cs
        dotnet build -o ../build/cs/
        echo "C# build complete."
        ;;
    
    "transpile")
        echo "Transpiling C# to C++..."
        mkdir -p generated
        
        # Find all .cs files in the cs directory, excluding build artifacts
        cs_files=$(find cs -name "*.cs" -type f -not -path "*/obj/*" -not -path "*/bin/*")
        
        if [ -z "$cs_files" ]; then
            echo "No .cs files found in cs/ directory."
            exit 1
        fi
        
        echo "Found C# files:"
        echo "$cs_files"
        
        # Transpile all .cs files in a single call
        echo "Transpiling all files..."
        miniscript tools/transpile.ms $cs_files
        if [ $? -ne 0 ]; then
            echo "Error: Failed to transpile C# files"
            exit 1
        fi
        
        echo "Transpilation complete."
        ;;
    
    "cpp")
        echo "Building C++ version..."
        if [ ! -f "generated/App.g.cpp" ]; then
            echo "Transpiled C++ files not found. Run 'transpile' first."
            exit 1
        fi
        case "$GOTO_MODE" in
            "on")
                echo "Forcing computed-goto ON"
                make -C cpp GOTO_MODE=on
                ;;
            "off")
                echo "Forcing computed-goto OFF"
                make -C cpp GOTO_MODE=off
                ;;
            *)
                echo "Using auto-detected computed-goto"
                make -C cpp
                ;;
        esac
        echo "C++ build complete."
        ;;
    
    "all")
        echo "Building all targets..."
        $0 cs
        $0 transpile
        $0 cpp "$GOTO_MODE"
        echo "All builds complete."
        ;;
    
    "clean")
        echo "Cleaning all build artifacts..."
        rm -rf build/cs/* build/cpp/* build/temp/*
        rm -rf generated/*.g.h generated/*.g.cpp
        cd cs && dotnet clean
        if [ -f cpp/Makefile ]; then
            make -C cpp clean
        fi
        echo "Clean complete."
        ;;
    
    "test")
        echo "Running quick smoke tests..."
        echo "Testing C# version:"
        cd build/cs && echo "These are some words for testing" | ./MS2Proto3
        cd ../..
        echo "Testing C++ version:"
        cd build/cpp && echo "These are some words for testing" | ./MS2Proto3
        cd ../..
        ;;

    "test-all")
        echo "Running all test suites..."
        make -C tests all
        ;;

    "test-cpp")
        echo "Running C++ test suites..."
        make -C tests cpp
        ;;

    "test-cs")
        echo "Running C# test suites..."
        make -C tests cs
        ;;

    "test-parser")
        echo "Running parser tests..."
        make -C tests parser
        ;;

    "test-vm")
        echo "Running VM tests..."
        make -C tests vm
        ;;

    *)
        echo "Usage: $0 {setup|parser|cs|transpile|cpp|all|clean|test|test-*} [goto_mode]"
        echo ""
        echo "Build Commands:"
        echo "  setup       - Set up development environment"
        echo "  parser      - Generate parser from grammar files"
        echo "  cs          - Build C# version only"
        echo "  transpile   - Transpile C# to C++"
        echo "  cpp         - Build C++ version only"
        echo "  all         - Build everything"
        echo "  clean       - Clean build artifacts"
        echo ""
        echo "Test Commands:"
        echo "  test        - Quick smoke test of built executables"
        echo "  test-all    - Run all test suites (C++ and C#)"
        echo "  test-cpp    - Run all C++ test suites"
        echo "  test-cs     - Run all C# test suites"
        echo "  test-parser - Run parser tests only"
        echo "  test-vm     - Run VM tests only"
        echo ""
        echo "Optional goto_mode for C++ builds:"
        echo "  auto        - Auto-detect computed-goto support (default)"
        echo "  on          - Force computed-goto ON"
        echo "  off         - Force computed-goto OFF"
        exit 1
        ;;
esac

echo "Build script completed successfully."