// value_string.h — string operations on NaN-boxed Values.
//
// Heap strings live in GCManager.Strings (a GCSet<GCString> with an
// associated intern side-table). Tiny strings (≤5 ASCII bytes) are
// encoded inline in the Value bits.

#ifndef VALUE_STRING_H
#define VALUE_STRING_H

#include "value.h"
#include <stdbool.h>

#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// Strings shorter than this are interned; longer ones bypass the intern
// table. The same threshold is used by GCManager.
#define INTERN_THRESHOLD 128

// ── Creation ────────────────────────────────────────────────────────────
Value make_string(const char* str);
Value make_string_n(const char* str, int len);
Value make_tiny_string(const char* str, int len);

// ── Access ──────────────────────────────────────────────────────────────
const char* as_cstring(Value v);     // null-terminated; valid until next GC
int  string_lengthB(Value v);         // byte length
int  string_length(Value v);          // Unicode character count

// Hot-path zero-copy access to underlying bytes. For tiny strings the
// returned pointer is into the Value itself; the caller must not let v
// move on the stack until done with the bytes.
const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len);

// Fill tiny_buffer (size ≥ TINY_STRING_MAX_LEN+1) for tiny strings;
// return a borrowed pointer into the heap string for heap strings.
const char* get_string_data_nullterm(const Value* v_ptr, char* tiny_buffer);

// ── Operations ──────────────────────────────────────────────────────────
bool  string_equals(Value a, Value b);
int   string_compare(Value a, Value b);
Value string_concat(Value a, Value b);
int   string_indexOf(Value haystack, Value needle, int start_pos);
Value string_replace(Value source, Value search, Value replacement);
Value string_replace_max(Value source, Value search, Value replacement, int maxCount);
Value string_split(Value str, Value delimiter);
Value string_split_max(Value str, Value delimiter, int maxCount);
Value string_substring(Value str, int startIndex, int len);
Value string_slice(Value str, int start, int end);
Value string_sub(Value a, Value b);
Value string_insert(Value str, int index, Value value, void* vm);
Value string_upper(Value str);
Value string_lower(Value str);
Value string_from_code_point(int codePoint);
int   string_code_point(Value str);

// ── Hashing ─────────────────────────────────────────────────────────────
uint32_t string_hash(const char* data, int len);
uint32_t get_string_hash(Value str_val);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VALUE_STRING_H
