# Zsh tab-completion for tools/build.sh
# Source this from ~/.zshrc:
#   source /path/to/miniscript2/tools/build_completion.zsh

_build_sh() {
    local state
    local -a subcommands cpp_opts goto_opts cs_files

    subcommands=(
        'setup:Set up development environment'
        'cs:Build C# version only'
        'transpile:Transpile C# to C++ (all files, or single file)'
        'cpp:Build C++ version only'
        'all:Build everything (cs + transpile + cpp)'
        'clean:Clean all build artifacts'
        'test:Quick smoke test of built executables'
        'test-all:Run all test suites (C++ and C#)'
        'test-cpp:Run all C++ test suites'
        'test-cs:Run all C# test suites'
        'test-vm:Run VM tests only'
        'xcode:Generate Xcode project in cpp/xcode/'
    )

    cpp_opts=(
        'debug:Build with -O0 -g and asserts enabled'
        'auto:Auto-detect computed-goto support (default)'
        'on:Force computed-goto ON'
        'off:Force computed-goto OFF'
    )

    # Determine project root relative to the build.sh location
    local script_dir
    script_dir="$(dirname "${words[1]}")"
    local cs_dir="$script_dir/../cs"

    if (( CURRENT == 2 )); then
        _describe 'subcommand' subcommands
    elif (( CURRENT >= 3 )); then
        case "${words[2]}" in
            transpile)
                # Complete .cs filenames (basename only, as the script prepends cs/)
                cs_files=( $(find "$cs_dir" -maxdepth 1 -name "*.cs" \
                    -not -path "*/obj/*" -not -path "*/bin/*" \
                    -exec basename {} \; 2>/dev/null) )
                _describe 'C# source file' cs_files
                ;;
            cpp|all)
                # Any combination of: debug, auto, on, off
                _describe 'build option' cpp_opts
                ;;
        esac
    fi
}

# Register the completion for any invocation of tools/build.sh
compdef _build_sh tools/build.sh
compdef _build_sh build.sh
