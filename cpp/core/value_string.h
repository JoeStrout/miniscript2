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

namespace MiniScript {


// Strings shorter than this are interned; longer ones bypass the intern
// table. The same threshold is used by GCManager.
#define INTERN_THRESHOLD 128

// ── Creation ────────────────────────────────────────────────────────────
// (make_string / as_cstring / string_length etc. are now Value:: static
// methods, declared in value.h.)
Value make_string_n(const char* str, int len);
Value make_tiny_string(const char* str, int len);

// ── Access ──────────────────────────────────────────────────────────────
int  string_lengthB(Value v);         // byte length

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
Value string_replace(Value source, Value search, Value replacement);
Value string_split(Value str, Value delimiter);
Value string_sub(Value a, Value b);
// string_indexOf / string_replace_max / string_split_max / string_substring /
// string_slice / string_insert / string_upper / string_lower /
// string_from_code_point / string_code_point are now Value:: static methods.

// ── Hashing ─────────────────────────────────────────────────────────────
uint32_t string_hash(const char* data, int len);
uint32_t get_string_hash(Value str_val);


}  // namespace MiniScript

#endif // VALUE_STRING_H
