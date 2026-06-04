# Opportunities for Improvement (OFI)

A survey of the C# (`cs/`) and C/C++ (`cpp/core/`) source for bad smells, inconsistencies, naming issues, and inefficiencies. Items are grouped by category; each is brief by design.

---

## Naming & Consistency

- **`Bytecode.cs` ~33**: Opcode `MULT_rA_rB_rC` uses `MULT` while all peers use three-letter forms — a TODO comment notes the rename. ✔️
- **`Bytecode.cs` ~258–259**: `ToMnemonic()` returns `"IFLT_rA_rB"` for *both* `IFNE_rA_rB` and `IFNE_rA_iBC` cases — copy-paste error. ✔️
- **`Disassembler.cs` ~54–55**: Returns `"BCTRUE"`/`"BCFALSE"` where all other branch-display strings use the `BR` prefix.
- **`Assembler.cs` ~190**: Comment laments scattered, inconsistent string↔Value conversions; no central helpers exist.
- **`Intrinsic.cs` ~29–31**: Short-name registry uses parallel `List<Value>` / `List<String>` instead of a single `Dictionary`; harder to keep in sync.

---

## Code Duplication

- **`CoreIntrinsics.cs` ~64–137**: Six type-builder blocks (`ListType`, `StringType`, etc.) follow identical `AddIntrinsicToMap()` boilerplate — a shared factory would halve the code.
- **`Assembler.cs` ~669–681, 783–824**: Branch-offset resolution and integer-parsing logic appear in multiple helper methods with near-identical logic.
- **`App.cs` ~438–563**: REPL input handling, history search, and recall parsing each contain overlapping number/string-parsing branches.
- **`Disassembler.cs` ~205–278**: Each switch-case repeats similar operand-formatting code; an extracted `FormatOperand()` helper would reduce noise.

---

## Performance & Inefficiency

- **`Assembler.cs` ~867**: `AddConstant()` does a linear scan for duplicates on every call; a `Dictionary` lookup would be O(1).
- **`Assembler.cs` ~889–906**: `NeedsConstant()` effectively parses an integer twice (once to detect, once to validate); a single parse pass suffices.
- **`CodeGenerator.cs` ~87**: `AllocConsecutiveRegs()` scans with an O(n) loop; a bitmap or free-list would scale better for large register windows.
- **`Intrinsic.cs` ~80–85**: `GetByName()` does a linear search every call despite the table being static after startup — should be a `Dictionary`. ✔️
- **`VM.cs` ~260–285**: `MarkRoots()` iterates full stack/names arrays even when mostly empty; tracking a high-water mark would avoid unnecessary work.

---

## Dead Code & Obsolete Patterns

- **`Value.cs` ~44–45**: `ToString()` is marked `[Obsolete]`, suggesting callers still exist that haven't been migrated. (Somewhat complicated by the standard inherited ToString.)
- **`Intrinsic.cs` ~87**: `AllCount()` has a comment asking whether it is redundant with `Count` — likely dead. ✔️
- **`Bytecode.cs` ~88**: `CALLFN_iA_kBC` is explicitly marked DEPRECATED; the opcode and any code paths handling it should be removed.
- **`Interpreter.cs` ~152**: Commented-out `parser.PartialReset()` call left as a TODO.
- **`CoreIntrinsics.cs` ~290–291**: `info()` intrinsic has a `// ToDo: return an error` comment on a null-argument path that silently does nothing.
- **`Assembler.cs` ~74, 79**: Commented-out error-raising code on invalid insert operations.

---

## API Clarity & Encapsulation

- **`Assembler.cs` ~18–32**: `Functions` and `Current` are exposed as public mutable fields; properties (possibly read-only) would make ownership clearer.  (May not be possible due to transpiling requirements.)
- **`CodeEmitter.cs` ~14–16**: Abstract base exposes `PendingFunc` as a public field mutated directly by subclasses rather than through methods.
- **`GCItems.cs` ~34–35, 73–74**: Constructor sets `Frozen = false` explicitly — harmless but implies the field is brittle if new fields are added without similar guards.
- **`FuncDef.cs` ~37–38**: RLE line-number encoding is entirely opaque; `AddInstruction()` is public but the encoding contract is private and undocumented.

---

## Error Handling & Robustness

- **`Assembler.cs` ~127**: On error, processing stops but partial state is not cleared; a subsequent call on the same assembler instance may see stale data.
- **`Assembler.cs` ~574, 589**: `(Byte)ParseInt16(parts[1])` casts without a bounds check; out-of-range values silently truncate.
- **`VM.cs` ~189**: `GetCallStackFrame()` returns `CallInfo(0, 0, null)` on an out-of-bounds index instead of throwing — silent failure.
- **`Lexer.cs` ~82**: TODO comment: "rework this whole file to be fully Unicode-savvy" — Unicode edge cases unhandled. (Turns out it was all Unicode-savvy already.)  ⛌
- **`VM.cs` ~123**: `_errorStackPending` flag is fragile if an error is raised during error handling itself.
- **`Parser.cs` ~220**: Returns a dummy `TokenType.ERROR` token but doesn't halt further parsing — subsequent errors may be misleading.  (Possibly irrelevant since we report only the first error.)

---

## Missing Documentation

- **`GCManager.cs` ~20–27**: No high-level comment explaining the overall GC design; the GCSet index semantics are inlined and cryptic.  (But see [ADR #4](adr/0004-GC-system.md).)
- **`VM.cs` ~31–70**: `CallInfo`'s iteration encoding (negative = VarMap, non-negative = Dict) is a subtle invariant with no doc comment.
- **`CodeEmitter.cs` ~141–169**: 24-bit offset encoding in the label-patching logic has no comment explaining the bit layout.
- **`GCItems.cs` ~176–189**: The `-(found + 2)` encoding for map iteration indices is clever but easy to get wrong; a brief comment would help.
- **`Value.cs` ~37–42**: NaN-boxing is well-commented internally, but the mapping of GCSet indices to semantic types is still cryptic to a newcomer.

---

## Magic Numbers & Hardcoded Limits

- **`GCManager.cs` ~33**: `InternThreshold = 128` — no explanation of the performance tradeoff that set this value.
- **`VM.cs` ~205**: Stack size defaults (`stackSlots=1024`, `callSlots=256`) are arbitrary with no comment.
- **`Assembler.cs` ~565**: The constant `16777215` (0xFFFFFF) for `ARG` range validation should be a named constant.
- **`CoreIntrinsics.cs` ~272**: Error cycle-detection depth of 256 is arbitrary and undocumented.
- **`StringUtils.cs` ~74**: `Spaces()` has no upper-bound guard; pathologically large values could allocate enormous strings.

---

## Subtle Logic Concerns

- **`Assembler.cs` ~1054–1056**: Label-counting logic increments for both NOOP and non-label lines, which may cause instruction-count mismatches.
- **`CodeGenerator.cs` ~86**: `_maxRegUsed` is updated only when the freed register equals the current max; it is not recalculated after nested scopes, potentially leaving it stale.
- **`Disassembler.cs` ~101**: Pads mnemonic to 7 chars, but opcodes like `METHFIND` (8 chars) overflow the padding silently.
- **`Assembler.cs` ~889–906**: Floating-point parsing in `NeedsConstant()` is unvalidated; malformed literals like `"1.2.3"` may pass silently.
- **`VM.cs`**: No visible guard against infinite recursion / stack overflow; deep MiniScript recursion will crash the host rather than raise a runtime error.

---

# Opportunities for Improvement — C/C++ Core (`cpp/core/`)

A survey of the hand-written C and C++ runtime layer.

---

## Naming & Clarity

- **Mixed naming conventions** (`CS_String.h`, `CS_String.cpp`): `ss_` (C-style), `Is`/`get` (camelCase), and `TINY_STRING_TAG` (UPPER_SNAKE) coexist with no obvious unifying scheme for getters or factory methods.  (May just need documentation: C# APIs must match C# exactly, and other styles are used to distinguish non-C# APIs.)
- **Ambiguous `data` name** (`CS_Dictionary.h` ~83–84): Both `DictionaryStorage` and `DictEntry` have a field named `data`, which is confusing when discussing the dictionary internals.
- **Conflated empty vs. unallocated strings** (`StringStorage.c` ~19–20, `StringStorage.h` ~31): Some functions return `NULL` for empty strings; others return valid storage with `lenB==0`. No clear contract.
- **Stale comment on `dispatch_macros.h`** (~8): Header comment says "layer checks" but the file is the VM dispatch system — documentation is simply wrong.
- **`TempStorage::_owned` semantics unclear** (`value_string.cpp` ~54–82): Flag is set only for tiny strings; heap strings alias borrowed memory but the naming gives no hint of this.

---

## Code Duplication

- **Massive duplicate case tables** (`unicodeUtil.c` ~17–169): `sUpperTable` and `sLowerTable` are ~600 lines each with no compression; a diff-encoded or combined table would be a fraction of the size.  (Without sacrificing performance?  No.)  ⛌
- **Repeated tiny-string bit-shifting** (`value_string.cpp` ~60–66, 117–142, etc.): Manual bit-shift logic for packing/unpacking tiny strings appears across `make_tiny_string`, `as_cstring`, `string_length`, and two `get_string_data_*` helpers; should be one set of inline helpers.
- **`TempStorage` materialization pattern** (`value_string.cpp`): Eight or more functions create a `TempStorage`, call `ss_*`, then wrap the result — identical boilerplate each time.
- **Repeated null/empty guard in list and map** (`value_list.cpp` ~40–44, `value_map.cpp` ~30–33): Every accessor begins with the same `if (!is_list/map(val)) return val_null` + GCManager lookup block.

---

## Performance & Inefficiency

- **Bubble sort** (`CS_List.h` ~230–244): O(n²) sort; should use `std::sort`.
- **Linear scan of case tables** (`unicodeUtil.c` ~174–211): 600-element tables searched linearly despite being sorted; binary search would cut time by ~9×.  (sLowerTable is not fully sorted; so we now dynamically allocate and sort a reverse copy.) ✔️
- **`ss_split` double-pass** (`StringStorage.c` ~428–456): Counts separators in one pass (~430) then splits in a second pass (~442); a single-pass approach with a growable result array would suffice.  (But might perform worse overall -- "growable result" means reallocation and copying.)
- **String concat overhead for tiny strings** (`value_string.cpp` ~195–205): `string_concat` allocates two `TempStorage` objects even when both strings are sub-5-byte tiny strings; the result could be produced inline.
- **No string pooling or slab allocation** (`StringStorage.c` ~18, 34, 39): Each string is individually `malloc`'d with a variable-length tail; a pool allocator for small strings would reduce fragmentation.

---

## Memory Safety & Error Handling

- **Potential infinite loop in `ss_replaceByte`** (`StringStorage.c` ~568): `while (*src)` loop body lacks an increment; infinite loop on any non-empty input.
- **UTF-8 buffer overread** (`StringStorage.c` ~87): `ss_codePointAt` passes `storage->data + byteIndex` to `UTF8Decode` without checking that enough bytes remain in the buffer for a multi-byte sequence.
- **Integer overflow on concat** (`StringStorage.c` ~243, `CS_String.h` ~562): `totalLen = storage->lenB + other->lenB` with no overflow check; concatenating two large strings can produce a smaller-than-expected allocation.
- **Unchecked `malloc` returns** (`CS_String.h` ~567, 617): `Join` methods do not validate the allocation before writing to it.
- **`strncpy` size risk** (`CS_String.h` ~756): `strncpy(formatSpec, format + start, len)` uses `len` rather than `sizeof(formatSpec) - 1`; could write beyond the 32-byte buffer.
- **Cast-away-const for lazy caching** (`StringStorage.c` ~65): `ss_lengthC` casts a `const StringStorage*` to mutable to cache `lenC`. This is a data race if two threads call `ss_lengthC` simultaneously on the same string.
- **Missing token-allocation failure handling in `ss_split`** (`StringStorage.c` ~445–449): If any individual token allocation fails, previously allocated tokens are leaked.

---

## Dead Code & Incomplete Implementation

- **Unimplemented string functions** (`StringStorage.c` ~465–480): `ss_insert`, `ss_remove`, `ss_removeLen`, and `ss_splitStr` all return `NULL` with TODO comments, yet are referenced from `CS_String.h`.
- **No-op map capacity functions** (`value_map.cpp` ~164–166): `map_needs_expansion` and `map_expand_capacity` are stubs with commented-out parameters; dead code that creates false confidence.
- **Uninimplemented intern TODO** (`CS_String.cpp` ~48–50): `FindOrCreate` has a TODO to intern strings under 128 bytes but does nothing; contradicts `INTERN_THRESHOLD` in `value_string.h`.  (But this code is _not_ about value strings; it's about shared_ptr strings, which would need a separate interning system if we decide it's worth it.)
- **Unused `fnv1a_hash` declaration** (`unicodeUtil.h` ~185): Declared but never implemented; abandoned in favour of `string_hash`.
- **Disabled layer check** (`layer_defs.h` ~76): `#define LAYER_2A_BSIDE 0` hard-codes the check off; either enforce it or remove the machinery.

---

## API Clarity & Consistency

- **Inconsistent error return values** (`StringStorage.c` throughout): Some functions return `NULL` on error, others return an empty-string object, others return the input unchanged; no documented convention.
- **`bool ascending` instead of enum** (`CS_List.h` ~230–244, `value_list.cpp` ~171): Sort direction should be an enum to make call sites readable and to allow future additions (e.g., case-insensitive).  (Or maybe not, since that's not how MiniScript sorting works.)
- **Null vs. empty treated inconsistently** (`CS_List.h` ~92–100, `CS_Dictionary.h` ~191–195): Both return 0 from `Count()` on a null object, but some code paths treat null and empty as distinct via `ensureData()`.
- **No allocator abstraction** (`CS_String.h` ~194, 208): `ss_concat` is called with bare `malloc`; swapping in a pool allocator later would require editing every call site.

---

## Risky Patterns & Undefined Behaviour

- **`thread_local` scratch buffer** (`value_string.cpp` ~114): `as_cstring` uses a `thread_local` buffer invalidated on the next call; dangerous if the caller stores or passes the pointer.
- **Cyclic `__isa` chain walk** (`value_map.cpp` ~64–75): Depth-limited to the magic number 256 but with no error reported when the limit is hit; silently returns wrong results on circular prototypes.  (Or is it wrong?  "No" will always be correct in this case.)
- **Endianness assumption without assertion** (`value_string.cpp` ~60–95): Tiny-string packing manually bit-shifts bytes assuming little-endian layout; `value.h` has an `__BYTE_ORDER__` check but `value_string.cpp` does not guard or assert.
- **Shadowed loop variable** (`value_string.cpp` ~221–222): Outer `i` and an inner `i` shadow each other; a compiler warning flag (`-Wshadow`) would catch this.

---

## Magic Numbers & Hardcoded Limits

- **`256` recursion depth** (`value.cpp` ~249, `value_map.cpp` ~64, and others): Three or more independent copies of this limit with no shared named constant and no explanation of the choice.
- **FNV constants inline** (`hashing.c` ~15–16): `FNV_PRIME` and `FNV_OFFSET_BASIS` are literal hex values with no reference to the spec; should be named constants with a comment.
- **Load factor fractions scattered** (`CS_Dictionary.h` ~163, 215): Resize threshold (¾) and capacity growth factor (4/3) are inline magic numbers rather than named constants.
- **Duplicate intern threshold** (`value_string.h` ~21, `CS_String.cpp` ~48): Two different files define 128 as the intern threshold; they could drift independently.  (Which would be fine as these are independent intern pools; but the latter is not implemented.  So, implement it or remove the stub code.)
- **Tiny-string offsets hardcoded** (`value_string.cpp` ~60–66): Expressions like `8 * (i + 1)` embed the tiny-string struct layout directly rather than referencing `TINY_STRING_MAX_LEN` or a sizeof.

---

## Macro & Preprocessor Concerns

- **Endianness-dependent `GET_VALUE_DATA_PTR`** (`value.h` ~195–201): Macro behaviour changes silently with `__BYTE_ORDER__`; callers must understand the endianness implication, which is not obvious at use sites.
- **Double include guards** (`StringStorage.h` ~1, `unicodeUtil.h` ~3): Both `#pragma once` and traditional `#ifndef` guards are present; pick one.
- **Multiple `extern "C"` blocks** (`value.cpp` ~30–31, 87–88, 98–99): Fragmented extern-C blocks in a single file; wrapping the whole file once is cleaner.  (But not possible due to the need for C++ linkage on some functions; maybe we give up on C linkage entirely.)

---

## Testing

- **Near-empty test file** (`test_simple.cpp`): Only exercises string creation; no coverage of `Dictionary`, `List`, UTF-8 edge cases, or any error path.
- **`UnicodeValidateCaseTables` result ignored** (`unicodeUtil.h` ~120): Validation function is declared but its return value is never checked at startup; a sorted binary-search implementation would require this validation.
