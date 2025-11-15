// String implementation for NaN-boxed Values.
// Supports both tiny strings (≤5 chars) and heap strings with interning.
// Uses GC-managed StringStorage for heap-allocated strings.

#ifndef STRINGS_H
#define STRINGS_H

#include "value.h"
#include "StringStorage.h"
#include <stdbool.h>

// This module is part of Layer 2A (Runtime Value System + GC)
#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif


// String creation
Value make_string(const char* str);
Value make_tiny_string(const char* str, int len);

// String access and conversion
const char* as_cstring(Value v);           // Get C string (may use internal buffer)
StringStorage* as_string(Value v);         // Get StringStorage struct (heap strings only)
int string_lengthB(Value v);               // Get byte length
int string_length(Value v);                // Get character length (Unicode-aware)

// String operations
bool string_equals(Value a, Value b);
int string_compare(Value a, Value b);          // Unicode-aware comparison
Value string_concat(Value a, Value b);
int string_indexOf(Value haystack, Value needle, int start_pos);
Value string_replace(Value source, Value search, Value replacement);
Value string_split(Value str, Value delimiter);
Value string_substring(Value str, int startIndex, int len);

// Zero-copy string data access (for performance-critical operations)
const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len);

// Null-terminated string data access (for C string operations)
const char* get_string_data_nullterm(const Value* v_ptr, char* tiny_buffer);

// String interning system
#define INTERN_THRESHOLD 128  // Strings under 128 bytes are automatically interned

// Hash function for strings
uint32_t string_hash(const char* data, int len);

// Get or compute hash value for a string
uint32_t get_string_hash(Value str_val);

// Accessor functions for debug output (used by gc_debug_output.c)
void* value_string_get_intern_table(void);
void* value_string_get_intern_entry_at(int bucket);
void* value_string_get_next_intern_entry(void* entry);
Value value_string_get_entry_value(void* entry);
int value_string_get_intern_table_size(void);
bool value_string_is_intern_table_initialized(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // STRINGS_H