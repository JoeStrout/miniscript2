// gc_debug_output.h - Debugging and diagnostic output for Layer 2A runtime
//
// This module contains all debugging/output functions for the runtime value
// system. It's separated from the core runtime to keep production code lean.

#ifndef GC_DEBUG_OUTPUT_H
#define GC_DEBUG_OUTPUT_H

#include "value.h"

#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// Value debugging
void debug_print_value(Value v);
const char* value_type_name(Value v);

// String debugging
void print_string_escaped(const char* str, int len, int max_len);
void dump_intern_table(void);

// GC debugging
void gc_dump_objects(void);
void gc_mark_and_report(void);

#ifdef __cplusplus
}
#endif


#endif // GC_DEBUG_OUTPUT_H
