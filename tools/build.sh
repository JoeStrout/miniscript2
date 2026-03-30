#!/bin/bash
# MiniScript2 build orchestration script

set -e  # Exit on any error

PROJECT_ROOT="$(dirname "$0")/.."
cd "$PROJECT_ROOT"

echo "=== MiniScript2 Build Script ==="
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

    "cs")
        echo "Building C# version..."
        cd cs
        dotnet build -o ../build/cs/
        echo "C# build complete."
        ;;
    
    "transpile")
        echo "Transpiling C# to C++..."
        mkdir -p generated

        # Check if a specific file was provided
        if [ -n "$2" ]; then
            # Single file transpile
            SINGLE_FILE="$2"
            echo "Transpiling single file: cs/$SINGLE_FILE"
            miniscript tools/transpile.ms -c cs -o generated "cs/$SINGLE_FILE"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to transpile cs/$SINGLE_FILE"
                exit 1
            fi
        else
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
        fi

        echo "Transpilation complete."
        ;;
    
    "cpp")
        echo "Building C++ version..."
        if [ ! -f "generated/App.g.cpp" ]; then
            echo "Transpiled C++ files not found. Run 'transpile' first."
            exit 1
        fi

        # Parse cpp sub-arguments: [debug] [goto_mode]
        BUILD_MODE="release"
        CPP_GOTO_MODE="auto"
        shift  # consume "cpp"
        for arg in "$@"; do
            case "$arg" in
                "debug")   BUILD_MODE="debug" ;;
                "on"|"off"|"auto") CPP_GOTO_MODE="$arg" ;;
            esac
        done

        MAKE_ARGS=""
        if [ "$BUILD_MODE" = "debug" ]; then
            echo "Building in DEBUG mode (-O0 -g, asserts enabled)"
            MAKE_ARGS="$MAKE_ARGS BUILD_MODE=debug"
        else
            echo "Building in RELEASE mode (-O3, asserts disabled)"
        fi

        case "$CPP_GOTO_MODE" in
            "on")
                echo "Forcing computed-goto ON"
                MAKE_ARGS="$MAKE_ARGS GOTO_MODE=on"
                ;;
            "off")
                echo "Forcing computed-goto OFF"
                MAKE_ARGS="$MAKE_ARGS GOTO_MODE=off"
                ;;
            *)
                echo "Using auto-detected computed-goto"
                ;;
        esac

        make -C cpp $MAKE_ARGS
        echo "C++ build complete."
        ;;
    
    "all")
        echo "Building all targets..."
        $0 cs
        $0 transpile
        shift  # consume "all"
        $0 cpp "$@"
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
        cd build/cs && echo "These are some words for testing" | ./miniscript2
        cd ../..
        echo "Testing C++ version:"
        cd build/cpp && echo "These are some words for testing" | ./miniscript2
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

    "test-vm")
        echo "Running VM tests..."
        make -C tests vm
        ;;

    "xcode")
        echo "Generating Xcode project..."
        if [ ! -f "cpp/CMakeLists.txt" ]; then
            echo "Error: cpp/CMakeLists.txt not found."
            exit 1
        fi
        mkdir -p cpp/xcode
        cd cpp/xcode
        cmake -G Xcode -DCMAKE_BUILD_TYPE=Debug ..
        echo "Xcode project generated at cpp/xcode/miniscript2.xcodeproj"
        echo "Open with: open cpp/xcode/miniscript2.xcodeproj"
        ;;

    *)
        echo "Usage: $0 {setup|cs|transpile|cpp|all|clean|test|test-*|xcode} [options]"
        echo ""
        echo "Build Commands:"
        echo "  setup       - Set up development environment"
        echo "  cs          - Build C# version only"
        echo "  transpile [file.cs] - Transpile C# to C++ (all files, or single file)"
        echo "  cpp [debug] [goto_mode] - Build C++ version only"
        echo "  all         - Build everything"
        echo "  clean       - Clean build artifacts"
        echo ""
        echo "Test Commands:"
        echo "  test        - Quick smoke test of built executables"
        echo "  test-all    - Run all test suites (C++ and C#)"
        echo "  test-cpp    - Run all C++ test suites"
        echo "  test-cs     - Run all C# test suites"
        echo "  test-vm     - Run VM tests only"
        echo ""
        echo "IDE:"
        echo "  xcode       - Generate Xcode project in cpp/xcode/"
        echo ""
        echo "C++ build options (can be combined in any order):"
        echo "  debug       - Build with -O0 -g and asserts enabled"
        echo "  auto        - Auto-detect computed-goto support (default)"
        echo "  on          - Force computed-goto ON"
        echo "  off         - Force computed-goto OFF"
        echo ""
        echo "Examples:"
        echo "  $0 cpp              # Release build, auto goto"
        echo "  $0 cpp debug        # Debug build, auto goto"
        echo "  $0 cpp debug on     # Debug build, computed-goto forced on"
        exit 1
        ;;
esac

echo "Build script completed successfully."