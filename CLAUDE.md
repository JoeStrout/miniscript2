# Claude Code Session Context for miniscrip2

## Project Overview
This is **MiniScript 2.0** - a dual-language (C#/C++) VM implementation using a transpiler-based architecture. Always start by reading [README.md](README.md) and the various .md documents in the `notes` folder for comprehensive project details.

**Key Architecture:**
- C# source code in `cs/` directory (primary development language)
- Transpiler converts C# → C++ (output in `generated/`)
- C/C++ core runtime in `cpp/core/` (memory management, Value types, etc.)
- Build system orchestrated by `tools/build.sh`

## Current Status: Milestone 3+ Complete
✅ **Completed Features:**
- Assembler/Disassembler for .msa (MiniScript Assembly) files
- Basic VM with arithmetic, comparison, branching, and function call support
- **Computed-goto dispatch** system with optimization flags
- **Function calls (CALLF/RETURN)** working correctly with proper context restoration
- NaN-boxed Value type system with garbage collection
- Dual memory management (GC for runtime values, MemPool for host app values)
- **Command-line interface** with `-debug` flag support
- **Comprehensive benchmark suite** for performance testing

🎯 **Current Milestone:** Extended VM (Milestone 4) - adding complete opcode set, maps, lists, closures

## Key Technical Implementations

### 1. Computed-Goto Dispatch System
**Location:** `cpp/core/dispatch_macros.h`, generated C++ VM code
- Uses X-macro pattern to automatically generate VM_LABEL_LIST from opcode enum
- Supports both computed-goto (GNU C extension) and switch-based dispatch
- Build system allows forcing dispatch method: `tools/build.sh cpp {auto|on|off}`
- **Performance benefit:** Direct jumps vs. switch statement overhead

### 2. Function Call/Return Mechanism
**Key insight:** Uses function indices instead of copying FuncDef objects for efficiency
- **CallInfo struct:** stores `ReturnPC`, `ReturnBase`, `ReturnFuncIndex` 
- **CALLF_iA_iBC:** saves caller context, switches to callee function by index
- **RETURN:** restores caller function using `functions[callInfo.ReturnFuncIndex]`
- **VM state:** tracks `currentFuncIndex` and `FuncDef& curFunc` (C++ uses reference to avoid copying)

### 3. Build System & Performance
**Main command:** `tools/build.sh {setup|cs|transpile|cpp|all|clean|test}`
- **Computed-goto control:** `tools/build.sh cpp {auto|on|off}` 
- **Auto-detection:** Uses GNU C extensions test, requires `-std=gnu++11` (not `-std=c++11`)
- **Optimization:** C++ builds with `-O3 -DNDEBUG` for production performance
- **Benchmark suite:** `tools/benchmark.sh` for performance comparison

## Important Files to Know

### Core Implementation
- **`cs/App.cs`** - Main host application (renamed from Program.cs)
- **`cs/VM.cs`** - Main VM implementation (C# source)
- **`generated/VM.g.cpp`** - Transpiled VM (auto-generated from C#)
- **`cpp/core/dispatch_macros.h`** - Computed-goto macros and opcode definitions
- **`cs/Bytecode.cs`** - Opcode definitions and bytecode utilities

### Build & Test
- **`tools/build.sh`** - Main build orchestration script
  - `tools/build.sh cs` - build C# project
  - `tools/build.sh transpile` - generate C++ code from C# files
  - `tools/build.sh cpp` - build C++ project
- **`tools/benchmark.sh`** - Performance benchmarking suite
- **`cpp/Makefile`** - C++ build configuration with optimization flags
- **`examples/*.msa`** - Assembly test programs (tuned for benchmarking)

## Recent Major Fixes & Optimizations

1. **Function Context Restoration (CALLF/RETURN):**
   - **Problem:** RETURN wasn't switching back to caller's function context
   - **Solution:** Store function index in CallInfo, restore via `functions[index]` lookup
   - **Optimization:** Use indices instead of copying FuncDef objects

2. **Computed-Goto Implementation:**
   - **Problem:** Missing VM_LABEL_LIST generation from opcode enum
   - **Solution:** X-macro pattern `VM_OPCODES(X)` mirrors C# enum structure
   - **Build fix:** Changed from `-std=c++11` to `-std=gnu++11` for GNU extensions

3. **Performance Optimization (Critical Fix):**
   - **Problem:** C++ was slower than C# due to missing optimization flags
   - **Solution:** Added `-O3 -DNDEBUG` to Makefile, now C++ significantly outperforms C#
   - **C++ version:** Uses `FuncDef& curFunc` reference instead of copying objects
   - **Function switching:** Minimal overhead with index-based lookups

4. **Command-Line Interface:**
   - **`-debug` flag:** Controls verbose output for development vs. clean benchmarking
   - **App class:** Renamed from Program to avoid naming conflicts with entry point
   - **Result highlighting:** Bold bright yellow output for benchmark results

## Common Commands

```bash
# Full build and test
tools/build.sh all && tools/build.sh test

# Performance benchmarking
tools/benchmark.sh                    # Compare C#, C++ (switch), C++ (goto)

# Test specific dispatch method
tools/build.sh cpp on   # Force computed-goto
tools/build.sh cpp off  # Force switch-based

# Run programs with different output modes
./build/cpp/MS2Proto3 examples/test_calls.msa           # Clean output
./build/cpp/MS2Proto3 -debug examples/test_calls.msa    # Verbose debug output
```

## Development Notes

- **Always edit C# files** in `cs/` directory, never generated C++ directly
- **Transpile after C# changes:** `tools/build.sh transpile`
- **Memory management:** Use GC_PROTECT for runtime Values, MemPool for host app data
- **Coding standards:** See [CS_CODING_STANDARDS.md](notes/CS_CODING_STANDARDS.md)

## Communication Style

Answer in a calm tone; avoid exclamation marks.  Be careful not to prematurely declare success; instead, plainly describe what you believe to be done.

Whenever you're not sure what is wanted, or how best to approach something, stop and ask for clarification.

---
*This context file helps maintain continuity across Claude Code sessions. Update as major features are implemented or architectural changes are made.*