#!/bin/bash

# MiniScript2 Benchmarking Script
# Runs performance benchmarks on both C# and C++ builds
# Verifies correctness of results

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Parse command line arguments
SELECTED_LANGUAGES="all"
while [[ $# -gt 0 ]]; do
    case $1 in
        -lang=*)
            SELECTED_LANGUAGES="${1#*=}"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [-lang=LANGUAGES]"
            echo "  -lang=LANGUAGES  Run specific languages (cs,cpp-switch,cpp-goto,msa,ms1,python,lua) or 'all' (default)"
            echo "  Each MS2 language (cs, cpp-switch, cpp-goto) runs both .msa (asm) and .ms (source) benchmarks"
            echo "  Benchmarks marked 'src' in BENCHMARKS have no .msa form; their asm cell shows '-'"
            echo "  Examples:"
            echo "    $0 -lang=msa"
            echo "    $0 -lang=msa,lua"
            echo "    $0 -lang=all"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Function to check if a language should be run
should_run_language() {
    local lang="$1"
    if [[ "$SELECTED_LANGUAGES" == "all" ]]; then
        return 0
    fi
    # Check if language is in comma-separated list
    if [[ ",$SELECTED_LANGUAGES," == *",$lang,"* ]]; then
        return 0
    fi
    return 1
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${BOLD}=== MiniScript2 Benchmark Suite ===${NC}"
echo "Project root: $PROJECT_ROOT"
echo ""

# Expected results for verification (allow for different scientific notation formats)
verify_result() {
    local actual="$1"
    local expected="$2"
    local name="$3"
    
    if [[ "$name" == "Iterative Factorial" ]]; then
        # For factorial, accept both full precision and scientific notation
        if [[ "$actual" == "2.43290200817664E+18" || "$actual" == "2.4329e+18" || "$actual" == "2432902008176640000" ]]; then
            return 0
        fi
    elif [[ "$actual" == "$expected" ]]; then
        return 0
    fi
    return 1
}


EXPECTED_ITER_FIB="832040"                 # fib(30) * 500000 iterations
EXPECTED_RECUR_FIB="3524578"              # fib(33)
EXPECTED_GLOBAL_LOOP="21534"              # see tools/benchmarks/global_loop.ms
EXPECTED_GLOBAL_CHURN="160238"            # see tools/benchmarks/global_churn.ms

# Benchmark definitions: file:Name:expected[:kinds]
#   kinds  "all" (default, when the field is absent) runs both the .msa and the
#          .ms form; "src" runs only the .ms form, for benchmarks that have no
#          hand-written assembly counterpart.
BENCHMARKS=(
    "factorial_iterative:Iterative Factorial:unused"
    "iter_fib:Iterative Fibonacci:$EXPECTED_ITER_FIB"
    "recur_fib:Recursive Fibonacci:$EXPECTED_RECUR_FIB"
    "global_loop:Global Loop:$EXPECTED_GLOBAL_LOOP:src"
    "global_loop_fn:Global Loop (locals):$EXPECTED_GLOBAL_LOOP:src"
    "global_churn:Global Churn:$EXPECTED_GLOBAL_CHURN:src"
)

# True if this benchmark has an assembly (.msa) form.
has_asm() {
    [[ "$1" != "src" ]]
}

# Render a timing for the summary table: "1.23s", or "-" for a skipped cell.
fmt_time() {
    if [[ "$1" == "-" || -z "$1" ]]; then echo "-"; else echo "$1s"; fi
}

# For quick testing, uncomment the line below to run only one benchmark:
# BENCHMARKS=("factorial_iterative.msa:Iterative Factorial:unused")

# Function to run a single benchmark and return timing
run_benchmark() {
    local benchmark_file="$1"
    local benchmark_name="$2"
    local expected_result="$3"
    local build_type="$4"
    local executable="$5"
    local file_ext="$6"

    local benchmark_path="tools/benchmarks/$benchmark_file.$file_ext"

    # Run benchmark and capture timing and result
    if [[ "$build_type" == "dotnet" ]]; then
        TIME_OUTPUT=$(time (dotnet "$executable" "$benchmark_path" 2>/dev/null) 2>&1)
        RESULT=$(dotnet "$executable" "$benchmark_path" 2>/dev/null | grep -m1 "Result in r0:" -A1 | tail -1 | sed 's/\x1b\[[0-9;]*m//g')
    else
        TIME_OUTPUT=$(time ("$executable" "$benchmark_path" 2>/dev/null) 2>&1)
        RESULT=$("$executable" "$benchmark_path" 2>/dev/null | grep -m1 "Result in r0:" -A1 | tail -1 | sed 's/\x1b\[[0-9;]*m//g')
    fi
    
    # Extract timing information (handle both formats: "real 0m3.072s" or "3.072 total")
    if echo "$TIME_OUTPUT" | grep -q "real"; then
        REAL_TIME=$(echo "$TIME_OUTPUT" | grep "real" | awk '{print $2}')
    else
        REAL_TIME=$(echo "$TIME_OUTPUT" | grep "total" | awk '{print $(NF-1)}')
    fi
    
    # Verify result - exit immediately if wrong
    if verify_result "$RESULT" "$expected_result" "$benchmark_name"; then
        echo "    ✓ $RESULT (${REAL_TIME})" >&2
    else
        echo "    ✗ FAILED: $RESULT (expected: $expected_result)" >&2
        echo "Benchmark failed - exiting." >&2
        exit 1
    fi
    
    # Convert timing to seconds and return just the number
    if [[ "$REAL_TIME" =~ ^([0-9]+)m([0-9.]+)s$ ]]; then
        # Format: 0m3.072s  
        minutes="${BASH_REMATCH[1]}"
        seconds="${BASH_REMATCH[2]}"
        echo "scale=3; $minutes * 60 + $seconds" | bc 2>/dev/null || echo "$seconds"
    else
        # Format: 3.072s or just 3.072
        echo "$REAL_TIME" | sed 's/s$//'
    fi
}

# Arrays to store results (asm = .msa, src = .ms)
declare -a CS_ASM_TIMES
declare -a CS_SRC_TIMES
declare -a CPP_GOTO_ASM_TIMES
declare -a CPP_GOTO_SRC_TIMES
declare -a CPP_SWITCH_ASM_TIMES
declare -a CPP_SWITCH_SRC_TIMES
declare -a MS1_TIMES
declare -a PY_TIMES
declare -a LUA_TIMES

echo -e "${BOLD}=== Benchmark Results ===${NC}"
echo ""

# Build and run all C# benchmarks
if should_run_language "cs"; then
    echo -e "${BOLD}Building C# version...${NC}"
    tools/build.sh cs > /dev/null
    echo -e "${BOLD}Running C# benchmarks (asm)...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"
        # No .msa form for this benchmark; keep the column aligned.
        if ! has_asm "$kinds"; then CS_ASM_TIMES+=("-"); continue; fi

        echo -e "${BLUE}  $name...${NC}"
        cs_time=$(run_benchmark "$file" "$name" "$expected" "dotnet" "build/cs/miniscript2.dll" "msa")
        CS_ASM_TIMES+=("$cs_time")
    done
    echo -e "${BOLD}Running C# benchmarks (source)...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"

        echo -e "${BLUE}  $name...${NC}"
        cs_time=$(run_benchmark "$file" "$name" "$expected" "dotnet" "build/cs/miniscript2.dll" "ms")
        CS_SRC_TIMES+=("$cs_time")
    done
    echo ""
fi

# Build and run all C++ (switch-based) benchmarks
if should_run_language "cpp-switch"; then
    echo -e "${BOLD}Building C++ version (switch-based)...${NC}"
    rm -f build/cpp/obj/* build/cpp/miniscript2 2>/dev/null
    if ! tools/build.sh cpp off; then
        echo -e "${RED}C++ (switch-based) build failed!${NC}"
        exit 1
    fi
    echo -e "${BOLD}Running C++ switch benchmarks (asm)...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"
        # No .msa form for this benchmark; keep the column aligned.
        if ! has_asm "$kinds"; then CPP_SWITCH_ASM_TIMES+=("-"); continue; fi

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "build/cpp/miniscript2" "msa")
        CPP_SWITCH_ASM_TIMES+=("$t")
    done
    echo -e "${BOLD}Running C++ switch benchmarks (source)...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "build/cpp/miniscript2" "ms")
        CPP_SWITCH_SRC_TIMES+=("$t")
    done
    echo ""
fi

# Build and run all C++ (computed-goto) benchmarks
if should_run_language "cpp-goto" || should_run_language "msa"; then
    echo -e "${BOLD}Building C++ version (computed-goto)...${NC}"
    rm -f build/cpp/obj/* build/cpp/miniscript2 2>/dev/null
    if ! tools/build.sh cpp on; then
        echo -e "${RED}C++ (computed-goto) build failed!${NC}"
        exit 1
    fi
    echo -e "${BOLD}Running C++ goto benchmarks (asm)...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"
        # No .msa form for this benchmark; keep the column aligned.
        if ! has_asm "$kinds"; then CPP_GOTO_ASM_TIMES+=("-"); continue; fi

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "build/cpp/miniscript2" "msa")
        CPP_GOTO_ASM_TIMES+=("$t")
    done
    echo -e "${BOLD}Running C++ goto benchmarks (source)...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "build/cpp/miniscript2" "ms")
        CPP_GOTO_SRC_TIMES+=("$t")
    done
    echo ""
fi

# Run all MiniScript 1.0 benchmarks
if should_run_language "ms1"; then
    echo -e "${BOLD}Running MiniScript 1.0 benchmarks...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "miniscript" "ms")
        MS1_TIMES+=("$t")
    done
    echo ""
fi

# Run all Python benchmarks
if should_run_language "python"; then
    echo -e "${BOLD}Running Python benchmarks...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "python" "py")
        PY_TIMES+=("$t")
    done
    echo ""
fi

# Run all Lua benchmarks
if should_run_language "lua"; then
    echo -e "${BOLD}Running Lua benchmarks...${NC}"
    for i in "${!BENCHMARKS[@]}"; do
        benchmark_def="${BENCHMARKS[i]}"
        IFS=':' read -r file name expected kinds <<< "$benchmark_def"

        echo -e "${BLUE}  $name...${NC}"
        t=$(run_benchmark "$file" "$name" "$expected" "direct" "lua" "lua")
        LUA_TIMES+=("$t")
    done
    echo ""
fi

# Dynamic summary table based on which languages were run
echo -e "${BOLD}=== Performance Summary ===${NC}"
echo ""

# Helper to add a column to the table layout
add_col() {
    local header="$1"
    local width="$2"
    table_header+="| $(printf "%-${width}s" "$header") "
    table_separator+="|$(printf '%0.s-' $(seq 1 $((width + 2))))"
    format_string+="| %-${width}s "
}

table_header="| Benchmark               "
table_separator="|-------------------------"
format_string="| %-23s "

if should_run_language "cs"; then
    add_col "C# asm" 8
    add_col "C# src" 8
fi
if should_run_language "cpp-switch"; then
    add_col "sw asm" 8
    add_col "sw src" 8
fi
if should_run_language "cpp-goto" || should_run_language "msa"; then
    add_col "goto asm" 8
    add_col "goto src" 8
fi
if should_run_language "ms1"; then
    add_col "MS 1.0" 8
fi
if should_run_language "python"; then
    add_col "Python" 8
fi
if should_run_language "lua"; then
    add_col "Lua" 8
fi

table_header+="|"
table_separator+="|"
format_string+="|\n"

echo "$table_header"
echo "$table_separator"

# Build dynamic table rows
for i in "${!BENCHMARKS[@]}"; do
    IFS=':' read -r file name expected kinds <<< "${BENCHMARKS[i]}"
    printf_args=("$name")

    if should_run_language "cs"; then
        printf_args+=("$(fmt_time "${CS_ASM_TIMES[i]}")")
        printf_args+=("$(fmt_time "${CS_SRC_TIMES[i]}")")
    fi
    if should_run_language "cpp-switch"; then
        printf_args+=("$(fmt_time "${CPP_SWITCH_ASM_TIMES[i]}")")
        printf_args+=("$(fmt_time "${CPP_SWITCH_SRC_TIMES[i]}")")
    fi
    if should_run_language "cpp-goto" || should_run_language "msa"; then
        printf_args+=("$(fmt_time "${CPP_GOTO_ASM_TIMES[i]}")")
        printf_args+=("$(fmt_time "${CPP_GOTO_SRC_TIMES[i]}")")
    fi
    if should_run_language "ms1"; then
        printf_args+=("$(fmt_time "${MS1_TIMES[i]}")")
    fi
    if should_run_language "python"; then
        printf_args+=("$(fmt_time "${PY_TIMES[i]}")")
    fi
    if should_run_language "lua"; then
        printf_args+=("$(fmt_time "${LUA_TIMES[i]}")")
    fi

    printf "$format_string" "${printf_args[@]}"
done

echo ""
echo -e "${GREEN}Benchmark suite completed!${NC}"