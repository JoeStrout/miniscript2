// gc_debug_output.c - Debugging and diagnostic output implementations

#include "gc_debug_output.h"
#include "value.h"
#include "value_string.h"
#include "gc.h"
#include "StringStorage.h"
#include <stdio.h>

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "gc_debug_output.c (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#if LAYER_2A_BSIDE
#error "gc_debug_output.c (Layer 2A - runtime) cannot depend on B-side layers (2B, 3B)"
#endif

// ===========================================================================
// Value debugging functions
// ===========================================================================

void debug_print_value(Value v) {
    if (is_null(v)) {
        printf("null");
    } else if (is_int(v)) {
        printf("int(%d)", as_int(v));
    } else if (is_double(v)) {
        printf("double(%g)", as_double(v));
    } else if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(&v);
        int len = (int)(unsigned char)data[0];
        printf("tiny_string(len=%d,\"", len);
        for (int i = 0; i < len && i < TINY_STRING_MAX_LEN; i++) {
            char c = data[1 + i];
            if (c >= 32 && c <= 126) {
                printf("%c", c);
            } else {
                printf("\\x%02x", (unsigned char)c);
            }
        }
        printf("\")");
    } else if (is_heap_string(v)) {
        uintptr_t ptr = (uintptr_t)(v & 0xFFFFFFFFFFFFULL);
        printf("heap_string(ptr=0x%llx)", (unsigned long long)ptr);
    } else if (is_list(v)) {
        uintptr_t ptr = (uintptr_t)(v & 0xFFFFFFFFFFFFULL);
        printf("list(ptr=0x%llx)", (unsigned long long)ptr);
    } else if (is_map(v)) {
        uintptr_t ptr = (uintptr_t)(v & 0xFFFFFFFFFFFFULL);
        printf("map(ptr=0x%llx)", (unsigned long long)ptr);
    } else {
        printf("unknown(0x%016llx)", v);
    }
}

const char* value_type_name(Value v) {
    if (is_null(v)) return "nil";
    if (is_int(v)) return "int";
    if (is_double(v)) return "double";
    if (is_tiny_string(v)) return "tiny_string";
    if (is_heap_string(v)) return "heap_string";
    if (is_list(v)) return "list";
    if (is_map(v)) return "map";
    return "unknown";
}

// ===========================================================================
// String debugging functions
// ===========================================================================

void print_string_escaped(const char* str, int len, int max_len) {
    for (int i = 0; i < len && i < max_len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c >= 32 && c < 127 && c != '"' && c != '\\') {
            printf("%c", c);
        } else if (c == '\n') {
            printf("\\n");
        } else if (c == '\r') {
            printf("\\r");
        } else if (c == '\t') {
            printf("\\t");
        } else if (c == '"') {
            printf("\\\"");
        } else if (c == '\\') {
            printf("\\\\");
        } else {
            printf("\\x%02x", c);
        }
    }
    if (len > max_len) {
        printf("...");
    }
}

// Access to intern table internals (declared in value_string.c)
extern void* value_string_get_intern_table(void);
extern void* value_string_get_intern_entry_at(int bucket);
extern void* value_string_get_next_intern_entry(void* entry);
extern Value value_string_get_entry_value(void* entry);
extern int value_string_get_intern_table_size(void);
extern bool value_string_is_intern_table_initialized(void);

void dump_intern_table(void) {
    if (!value_string_is_intern_table_initialized()) {
        printf("\n=== Intern Table ===\n");
        printf("Not initialized.\n");
        return;
    }

    int table_size = value_string_get_intern_table_size();
    printf("\n=== Intern Table ===\n");
    printf("Table size: %d buckets\n", table_size);

    int total_entries = 0;
    int used_buckets = 0;
    int max_chain_length = 0;

    // Count entries and analyze distribution
    for (int i = 0; i < table_size; i++) {
        void* entry = value_string_get_intern_entry_at(i);
        if (entry != NULL) {
            used_buckets++;
            int chain_length = 0;
            while (entry != NULL) {
                chain_length++;
                total_entries++;
                entry = value_string_get_next_intern_entry(entry);
            }
            if (chain_length > max_chain_length) {
                max_chain_length = chain_length;
            }
        }
    }

    printf("Total interned strings: %d\n", total_entries);
    printf("Used buckets: %d / %d\n", used_buckets, table_size);
    printf("Max chain length: %d\n", max_chain_length);
    printf("Avg chain length: %.2f\n",
           used_buckets > 0 ? (float)total_entries / used_buckets : 0.0f);
    printf("\nInterned strings:\n");

    // Dump all interned strings
    int string_num = 0;
    for (int bucket = 0; bucket < table_size; bucket++) {
        void* entry = value_string_get_intern_entry_at(bucket);
        while (entry != NULL) {
            string_num++;

            Value str_val = value_string_get_entry_value(entry);
            if (is_heap_string(str_val)) {
                StringStorage* str = as_string(str_val);
                if (str) {
                    printf("  [%d] bucket=%d hash=0x%08x len=%d \"",
                           string_num, bucket, str->hash, str->lenB);
                    print_string_escaped(str->data, str->lenB, 60);
                    printf("\"\n");
                }
            } else {
                printf("  [%d] bucket=%d (not a heap string?)\n", string_num, bucket);
            }

            entry = value_string_get_next_intern_entry(entry);
        }
    }

    if (total_entries == 0) {
        printf("  (no interned strings)\n");
    }

    printf("\n=== End Intern Table ===\n");
}

// ===========================================================================
// GC debugging functions
// ===========================================================================

// Access to GC internals (declared in gc.h)
// Note: Most stats come from gc_get_stats(), these are just for traversing structures
extern void* gc_get_all_objects(void);
extern void* gc_get_next_object(void* obj);
extern size_t gc_get_object_size(void* obj);
extern bool gc_is_object_marked(void* obj);
extern void gc_mark_phase(void);

static void print_hex_ascii_line(const unsigned char* data, size_t offset, size_t data_size) {
    // Print offset
    printf("  %04zx: ", offset);

    // Print hex bytes (16 per line)
    for (size_t i = 0; i < 16; i++) {
        if (i < data_size) {
            printf("%02x ", data[offset + i]);
        } else {
            printf("   ");
        }
        if (i == 7) printf(" ");  // Extra space in middle
    }

    printf(" |");

    // Print ASCII representation
    for (size_t i = 0; i < 16 && i < data_size; i++) {
        unsigned char c = data[offset + i];
        if (c >= 32 && c < 127) {
            printf("%c", c);
        } else {
            printf(".");
        }
    }

    printf("|\n");
}

void gc_dump_objects(void) {
    GCStats stats = gc_get_stats();

    printf("\n=== GC Objects Dump ===\n");
    printf("Total allocated: %zu bytes\n", stats.bytes_allocated);
    printf("GC threshold: %zu bytes\n", stats.gc_threshold);
    printf("Collections: %d\n\n", stats.collections_count);

    int object_count = 0;
    void* obj = gc_get_all_objects();

    while (obj) {
        object_count++;

        // Get pointer to actual data (after GCObject header)
        size_t obj_size = gc_get_object_size(obj);
        size_t header_size = 16;  // sizeof(GCObject) - hardcoded to avoid exposing struct
        unsigned char* data = (unsigned char*)obj + header_size;
        size_t data_size = obj_size - header_size;

        printf("Object #%d @ %p:\n", object_count, (void*)obj);
        printf("  Size: %zu bytes (+ %zu header = %zu total)\n",
               data_size, header_size, obj_size);
        printf("  Marked: %s\n", gc_is_object_marked(obj) ? "YES" : "no");

        // Try to heuristically identify if this looks like a StringStorage
        if (data_size >= sizeof(StringStorage)) {
            StringStorage* maybe_str = (StringStorage*)data;
            // Check if lengths are reasonable and hash is non-zero
            if (maybe_str->lenB >= 0 && maybe_str->lenB < 10000 &&
                maybe_str->lenC >= -1 && maybe_str->lenC <= maybe_str->lenB &&
                maybe_str->hash != 0) {
                printf("  Type hint: Possibly StringStorage (lenB=%d, lenC=%d, hash=0x%08x)\n",
                       maybe_str->lenB, maybe_str->lenC, maybe_str->hash);
                printf("  String data: \"");
                print_string_escaped(maybe_str->data, maybe_str->lenB, 60);
                printf("\"\n");
            }
        }

        // Hex/ASCII dump (first 64 bytes or less)
        size_t dump_size = data_size < 64 ? data_size : 64;
        printf("  Data (first %zu bytes):\n", dump_size);

        for (size_t offset = 0; offset < dump_size; offset += 16) {
            print_hex_ascii_line(data, offset, data_size);
        }

        if (data_size > 64) {
            printf("  ... (%zu more bytes)\n", data_size - 64);
        }

        printf("\n");
        obj = gc_get_next_object(obj);
    }

    if (object_count == 0) {
        printf("No GC objects allocated.\n");
    } else {
        printf("Total objects: %d\n", object_count);
    }
}

void gc_mark_and_report(void) {
    GCStats stats = gc_get_stats();

    printf("\n=== GC Mark and Report ===\n");
    printf("Running mark phase from %d roots...\n", stats.root_count);

    // Run mark phase (but NOT sweep)
    gc_mark_phase();

    printf("\n=== Reachability Report ===\n");

    int marked_count = 0;
    int unmarked_count = 0;
    size_t marked_bytes = 0;
    size_t unmarked_bytes = 0;

    void* obj = gc_get_all_objects();
    while (obj) {
        size_t obj_size = gc_get_object_size(obj);
        if (gc_is_object_marked(obj)) {
            marked_count++;
            marked_bytes += obj_size;
        } else {
            unmarked_count++;
            unmarked_bytes += obj_size;
        }
        obj = gc_get_next_object(obj);
    }

    printf("Reachable (marked): %d objects, %zu bytes\n", marked_count, marked_bytes);
    printf("Unreachable (unmarked): %d objects, %zu bytes\n", unmarked_count, unmarked_bytes);
    printf("\nReachable objects:\n");

    // Dump marked objects
    int obj_num = 0;
    obj = gc_get_all_objects();
    while (obj) {
        if (gc_is_object_marked(obj)) {
            obj_num++;
            size_t header_size = 16;  // sizeof(GCObject)
            unsigned char* data = (unsigned char*)obj + header_size;
            size_t obj_size = gc_get_object_size(obj);
            size_t data_size = obj_size - header_size;

            printf("\n#%d @ %p: %zu bytes\n", obj_num, (void*)obj, data_size);

            // Try to identify type
            if (data_size >= sizeof(StringStorage)) {
                StringStorage* maybe_str = (StringStorage*)data;
                if (maybe_str->lenB >= 0 && maybe_str->lenB < 10000 &&
                    maybe_str->lenC >= -1 && maybe_str->lenC <= maybe_str->lenB) {
                    printf("  [StringStorage: lenB=%d, hash=0x%08x] \"",
                           maybe_str->lenB, maybe_str->hash);
                    print_string_escaped(maybe_str->data, maybe_str->lenB, 60);
                    printf("\"\n");
                }
            }
        }
        obj = gc_get_next_object(obj);
    }

    printf("\n=== End Report ===\n");
    printf("Note: Marks remain set. Run gc_collect() to sweep and clear marks.\n");
}
