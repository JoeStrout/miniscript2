// nanbox_strings.c
//
// String operations for the NaN-boxing implementation
// These functions are too complex to be inlined and need proper GC management

#include "value_string.h"
#include "StringStorage.h"
#include "value.h"
#include "gc.h"
#include "unicodeUtil.h"
#include "hashing.h"
#include "value_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "value_string.c (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#if LAYER_2A_BSIDE
#error "value_string.c (Layer 2A - runtime) cannot depend on B-side layers (2B, 3B)"
#endif

// String interning system
#define INTERN_THRESHOLD 128  // Intern strings under 128 bytes
#define INTERN_TABLE_SIZE 1024  // Hash table size (must be power of 2)

// Hash table entry for string interning
typedef struct InternEntry {
    Value string_value;      // The interned string (heap string Value)
    struct InternEntry* next;  // Collision chain
} InternEntry;

// Global intern table
static InternEntry* intern_table[INTERN_TABLE_SIZE];
static bool intern_table_initialized = false;

// Initialize intern table
static void init_intern_table() {
    if (intern_table_initialized) return;
    for (int i = 0; i < INTERN_TABLE_SIZE; i++) {
        intern_table[i] = NULL;
    }
    intern_table_initialized = true;
}

// Buffer for tiny string conversion - thread-local would be better in real code
static char tiny_string_buffer[TINY_STRING_MAX_LEN + 1];

// String accessor functions
Value make_tiny_string(const char* str, int len) {
    // Create the value with proper type mask
    Value v = TINY_STRING_TAG;
    if (len == 0) return v;		// bail out if it's an empty string
    
    // Get pointer to the 6-byte data area and store length-prefixed string
    char* data = GET_VALUE_DATA_PTR(&v);
    data[0] = (char)len;  // Store length in first byte
    
    // Copy string data starting at byte 1
    for (int i = 0; i < len && i < TINY_STRING_MAX_LEN; i++) {
        data[1 + i] = str[i];
    }
    
    // Remaining bytes are already zero from TINY_STRING_TAG initialization
    // INVARIANT: All unused payload bytes in tiny strings are guaranteed to be zero
    
    return v;
}

StringStorage* as_string(Value v) {
    if (is_heap_string(v)) {
        return (StringStorage*)(uintptr_t)(v & 0xFFFFFFFFFFFFULL);
    }
    return NULL; // Tiny strings don't have a String structure
}

const char* as_cstring(Value v) {
    if (!is_string(v)) return NULL;
    
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(&v);
        int len = (int)(unsigned char)data[0];
        
        // Copy to buffer and add null terminator
        for (int i = 0; i < len && i < TINY_STRING_MAX_LEN; i++) {
            tiny_string_buffer[i] = data[1 + i];
        }
        tiny_string_buffer[len] = '\0';
        return tiny_string_buffer;
    } else {
        StringStorage* s = as_string(v);
        return ss_getCString(s);
    }
}

int string_lengthB(Value v) {
    if (!is_string(v)) return 0;
    
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(&v);
        return (int)(unsigned char)data[0];  // Byte length from first byte
    } else {
        StringStorage* s = as_string(v);
        return ss_lengthB(s);
    }
}

int string_length(Value v) {
    if (!is_string(v)) return 0;
    
    int lenB = string_lengthB(v);
    if (lenB == 0) return 0;
    
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(&v);
        return UTF8CharacterCount((const unsigned char*)(data + 1), lenB);
    } else {
        StringStorage* s = as_string(v);
        return ss_lengthC(s);  // Use cached character count
    }
}

const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len) {
    Value v = *v_ptr;
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(v_ptr);
        *out_len = (int)(unsigned char)data[0];
        return data + 1;  // Return pointer directly to string data (no copying!)
    } else if (is_heap_string(v)) {
        StringStorage* s = (StringStorage*)(uintptr_t)(v & 0xFFFFFFFFFFFFULL);
        *out_len = ss_lengthB(s);
        return ss_getCString(s);
    }
    *out_len = 0;
    return NULL;
}

const char* get_string_data_nullterm(const Value* v_ptr, char* tiny_buffer) {
    Value v = *v_ptr;
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(v_ptr);
        int len = (int)(unsigned char)data[0];
        
        // Optimization: if len < TINY_STRING_MAX_LEN, the data is already null-terminated
        // due to our invariant that unused bytes are zero
        if (len < TINY_STRING_MAX_LEN) {
            return data + 1;  // Return pointer directly into Value - already null-terminated!
        }
        
        // Only copy if string uses all 5 characters (no guaranteed null terminator)
        for (int i = 0; i < len; i++) {
            tiny_buffer[i] = data[1 + i];
        }
        tiny_buffer[len] = '\0';
        return tiny_buffer;
    } else if (is_heap_string(v)) {
        StringStorage* s = (StringStorage*)(uintptr_t)(v & 0xFFFFFFFFFFFFULL);
        return ss_getCString(s);
    }
    return NULL;
}

// Find or create an interned string
// Returns existing interned string if found, or NULL if not found
static Value find_interned_string(const char* data, int lenB, uint32_t hash) {
    init_intern_table();
    
    int bucket = hash & (INTERN_TABLE_SIZE - 1);  // hash % table_size
    InternEntry* entry = intern_table[bucket];
    
    while (entry != NULL) {
        if (is_heap_string(entry->string_value)) {
            StringStorage* s = as_string(entry->string_value);
            if (ss_lengthB(s) == lenB && s->hash == hash && memcmp(ss_getCString(s), data, lenB) == 0) {
                return entry->string_value;  // Found existing interned string
            }
        }
        entry = entry->next;
    }
    
    return make_null();  // Not found
}

// Add a string to the intern table
// The string_value must be a heap string with hash already computed
static void intern_string(Value string_value) {
    if (!is_heap_string(string_value)) return;
    
    StringStorage* s = as_string(string_value);
    if (s->hash == 0) return;  // Hash must be computed
    
    init_intern_table();
    
    int bucket = s->hash & (INTERN_TABLE_SIZE - 1);
    
    // Allocate new entry (this is not GC'd - it's part of the intern table)
    InternEntry* new_entry = malloc(sizeof(InternEntry));
    new_entry->string_value = string_value;
    new_entry->next = intern_table[bucket];
    intern_table[bucket] = new_entry;
}

// Make a Value string from a const char *.  This will create a tiny string,
// an interned string, or a GC heap-allocated string, depending on the length.
Value make_string(const char* str) {
    if (str == NULL) return make_null();
    int lenB = strlen(str);
    
    // Use tiny string for very short strings
    if (lenB <= TINY_STRING_MAX_LEN) {
        return make_tiny_string(str, lenB);
    }
    
    // Use interning for strings under threshold
    if (lenB < INTERN_THRESHOLD) {
        // Compute hash for interning
        uint32_t hash = string_hash(str, lenB);
        
        // Check if already interned
        Value existing = find_interned_string(str, lenB, hash);
        if (!is_null(existing)) {
            return existing;
        }
        
        // Create new interned string with pool allocator (not GC'd - immortal by design)
		// (ToDo: maybe use a MemPool allocator instead, so we can free them eventually?)
        StringStorage* s = ss_create(str, malloc);
        s->hash = hash;  // Store computed hash
        Value new_string = STRING_TAG | ((uintptr_t)s & 0xFFFFFFFFFFFFULL);
        
        // Add to intern table
        intern_string(new_string);
        
        return new_string;
    } else {
        // For longer strings, use regular GC heap allocation
        StringStorage* s = (StringStorage*)gc_allocate(sizeof(StringStorage) + lenB + 1);
        s->lenB = lenB;
        s->lenC = -1; // Compute character count later when needed
        s->hash = 0;  // ...and same for hash
        strcpy(s->data, str);
        return STRING_TAG | ((uintptr_t)s & 0xFFFFFFFFFFFFULL);
    }
}

// String equality with optimization for interned/identical strings
bool string_equals(Value a, Value b) {
    if (!is_string(a) || !is_string(b)) return false;
    
    // If these strings are identical pointers, then they must be equal
    if (a == b) return true;
    
    // Fast path: both tiny strings - compare entire Values directly
    if (is_tiny_string(a) && is_tiny_string(b)) {
        // Since both have identical type masks and our invariant guarantees
        // unused bytes are zero, so if they were identical tiny strings, they
        // would have identical values.  But we already checked for that, so:
        return false;
    }
    
    // Mixed or heap strings: use zero-copy comparison
    int len_a, len_b;
    const char* str_a = get_string_data_zerocopy(&a, &len_a);
    const char* str_b = get_string_data_zerocopy(&b, &len_b);
    
    if (len_a != len_b) return false;
    return memcmp(str_a, str_b, len_a) == 0;
}


Value string_concat(Value a, Value b) {
    GC_PUSH_SCOPE();
    
    Value result = make_null();
    GC_PROTECT(&a);
    GC_PROTECT(&b);
    GC_PROTECT(&result);
    
    // Use TRUE zero-copy access - no copying for tiny strings!
    int lenB_a, lenB_b;
    const char* sa = get_string_data_zerocopy(&a, &lenB_a);
    const char* sb = get_string_data_zerocopy(&b, &lenB_b);
    
    if (!sa || !sb) {
        GC_POP_SCOPE();
        return make_null();
    }
    
    int total_lenB = lenB_a + lenB_b;
    
    // Use tiny string if result is small enough (in bytes)
    if (total_lenB <= TINY_STRING_MAX_LEN) {
        char result_buffer[TINY_STRING_MAX_LEN + 1];
        memcpy(result_buffer, sa, lenB_a);
        memcpy(result_buffer + lenB_a, sb, lenB_b);
        result_buffer[total_lenB] = '\0';
        result = make_tiny_string(result_buffer, total_lenB);
    } else {
        // Use heap string for longer results
        StringStorage* result_str = (StringStorage*)gc_allocate(sizeof(StringStorage) + total_lenB + 1);
        result_str->lenB = total_lenB;
        result_str->lenC = -1;  // Will be computed when needed
        result_str->hash = 0;  // Hash not computed yet
        memcpy(result_str->data, sa, lenB_a);
        memcpy(result_str->data + lenB_a, sb, lenB_b);
        result_str->data[total_lenB] = '\0';
        result = STRING_TAG | ((uintptr_t)result_str & 0xFFFFFFFFFFFFULL);
    }
    
    GC_POP_SCOPE();
    return result;
}

Value string_replace(Value str, Value from, Value to) {
    GC_PUSH_SCOPE();
    
    Value result = make_null();
    GC_PROTECT(&str);
    GC_PROTECT(&from);
    GC_PROTECT(&to);
    GC_PROTECT(&result);
    
    int from_lenB = string_lengthB(from);
    int to_lenB = string_lengthB(to);
    int str_lenB = string_lengthB(str);
    
    if (from_lenB == 0 || str_lenB == 0) {
        GC_POP_SCOPE();
        return str; // Can't replace empty string, or nothing to replace in empty string
    }
    
    // Get null-terminated versions of the strings
    char tiny_buffer_s[TINY_STRING_MAX_LEN + 1];
    char tiny_buffer_f[TINY_STRING_MAX_LEN + 1];  
    char tiny_buffer_t[TINY_STRING_MAX_LEN + 1];
    
    const char* s = get_string_data_nullterm(&str, tiny_buffer_s);
    const char* f = get_string_data_nullterm(&from, tiny_buffer_f);
    const char* t = get_string_data_nullterm(&to, tiny_buffer_t);
    
    if (!s || !f || !t) {
        GC_POP_SCOPE();
        return make_null();
    }
    
    // Count occurrences to calculate final length
    int count = 0;
    const char* temp = s;
    while ((temp = strstr(temp, f)) != NULL) {
        count++;
        temp += from_lenB;
    }
    
    if (count == 0) {
        GC_POP_SCOPE();
        return str; // Return original if not found
    }
    
    // Calculate new byte length
    int new_lenB = str_lenB + count * (to_lenB - from_lenB);
    
    // Use static buffer for small results, heap for larger ones
    char static_buffer[256];
    char* temp_result;
    bool use_static = ((size_t)new_lenB < sizeof(static_buffer));
    
    if (use_static) {
        temp_result = static_buffer;
    } else {
        temp_result = malloc(new_lenB + 1);
    }
    
    // Build the result string
    char* dest = temp_result;
    const char* src = s;
    
    while (*src) {
        const char* found = strstr(src, f);
        if (found == NULL) {
            strcpy(dest, src);
            break;
        }
        
        // Copy text before the match
        int before_len = found - src;
        strncpy(dest, src, before_len);
        dest += before_len;
        
        // Copy replacement text
        strcpy(dest, t);
        dest += to_lenB;
        
        // Move source pointer past the match
        src = found + from_lenB;
    }
    
    result = make_string(temp_result);
    
    // Clean up heap allocation if used
    if (!use_static) {
        free(temp_result);
    }
    
    GC_POP_SCOPE();
    return result;
}

Value string_split(Value str, Value delimiter) {
    GC_PUSH_SCOPE();
    
    Value result = make_null();
    GC_PROTECT(&str);
    GC_PROTECT(&delimiter);
    GC_PROTECT(&result);
    
    int str_lenB = string_lengthB(str);
    int delim_lenB = string_lengthB(delimiter);
    
    if (str_lenB == 0) {
        result = make_list(0);
        GC_POP_SCOPE();
        return result;
    }
    
    // Get string data
    char tiny_buffer_s[TINY_STRING_MAX_LEN + 1];
    const char* s = get_string_data_nullterm(&str, tiny_buffer_s);
    if (!s) {
        GC_POP_SCOPE();
        return make_null();
    }
    
    bool has_delimiter = (delim_lenB > 0);
    const char* delim = NULL;
    char tiny_buffer_delim[TINY_STRING_MAX_LEN + 1];
    
    if (has_delimiter) {
        delim = get_string_data_nullterm(&delimiter, tiny_buffer_delim);
        if (!delim) {
            GC_POP_SCOPE();
            return make_null();
        }
    }
    
    // Handle empty delimiter (split into characters)
    if (!has_delimiter || strlen(delim) == 0) {
        int charCount = string_length(str);  // Use optimized character count
        result = make_list(charCount);
        
        unsigned char* ptr = (unsigned char*)s;
        unsigned char* end = ptr + str_lenB;
        
        while (ptr < end) {
            unsigned char* charStart = ptr;
            UTF8DecodeAndAdvance(&ptr);
            
            // Create string from this character
            int charLenB = ptr - charStart;
            char charBuffer[5];  // Max UTF-8 character is 4 bytes + null
            memcpy(charBuffer, charStart, charLenB);
            charBuffer[charLenB] = '\0';
            
            Value char_val = make_string(charBuffer);
            list_push(result, char_val);
        }
    }
    // Handle space delimiter as special case - manual split to preserve empty tokens
    else if (strcmp(delim, " ") == 0) {
        result = make_list(100); // Rough estimate
        int start = 0;
        
        for (int i = 0; i <= str_lenB; i++) {
            if (i == str_lenB || s[i] == ' ') {
                // Found delimiter or end of string
                int token_len = i - start;
                char token_buffer[256];
                if ((size_t)token_len < sizeof(token_buffer)) {
                    strncpy(token_buffer, s + start, token_len);
                    token_buffer[token_len] = '\0';
                    Value token_val = make_string(token_buffer);
                    list_push(result, token_val);
                } else {
                    char* token = malloc(token_len + 1);
                    strncpy(token, s + start, token_len);
                    token[token_len] = '\0';
                    Value token_val = make_string(token);
                    list_push(result, token_val);
                    free(token);
                }
                start = i + 1;
            }
        }
    }
    // General case: split on specific delimiter
    else {
        result = make_list(50); // Rough estimate
        char s_buffer[256];
        char* s_copy;
        bool use_static = ((size_t)str_lenB < sizeof(s_buffer));
        
        if (use_static) {
            strcpy(s_buffer, s);
            s_copy = s_buffer;
        } else {
            s_copy = malloc(str_lenB + 1);
            strcpy(s_copy, s);
        }
        
        char* token = strtok(s_copy, delim);
        while (token != NULL) {
            Value token_val = make_string(token);
            list_push(result, token_val);
            token = strtok(NULL, delim);
        }
        
        if (!use_static) {
            free(s_copy);
        }
    }
    
    GC_POP_SCOPE();
    return result;
}

Value string_substring(Value str, int startIndex, int len) {
    GC_PUSH_SCOPE();
    
    Value result = make_null();
    GC_PROTECT(&str);
    GC_PROTECT(&result);
    
    if (!is_string(str) || startIndex < 0 || len < 0) {
        GC_POP_SCOPE();
        return make_null();
    }
    
    int strLenB = string_lengthB(str);
    if (strLenB == 0) {
        result = val_empty_string;
        GC_POP_SCOPE();
        return result;
    }
    
    // Get string data
    char tiny_buffer[TINY_STRING_MAX_LEN + 1];
    const char* data = get_string_data_nullterm(&str, tiny_buffer);
    if (!data) {
        GC_POP_SCOPE();
        return make_null();
    }
    
    // Convert character indexes to byte indexes
    int startByteIndex = UTF8CharIndexToByteIndex((const unsigned char*)data, startIndex, strLenB);
    if (startByteIndex < 0) {
        result = val_empty_string;
        GC_POP_SCOPE();
        return result;
    }
    
    int endCharIndex = startIndex + len;
    int endByteIndex = UTF8CharIndexToByteIndex((const unsigned char*)data, endCharIndex, strLenB);
    if (endByteIndex < 0) endByteIndex = strLenB;  // Use end of string if out of range
    
    int subLenB = endByteIndex - startByteIndex;
    if (subLenB <= 0) {
        result = val_empty_string;
        GC_POP_SCOPE();
        return result;
    }
    
    // Create substring
    char* subBuffer = malloc(subLenB + 1);
    memcpy(subBuffer, data + startByteIndex, subLenB);
    subBuffer[subLenB] = '\0';
    
    result = make_string(subBuffer);
    free(subBuffer);
    
    GC_POP_SCOPE();
    return result;
}

Value string_charAt(Value str, int index) {
    return string_substring(str, index, 1);
}

// Unicode-aware string comparison
// Returns: < 0 if a < b, 0 if a == b, > 0 if a > b
int string_compare(Value a, Value b) {
    if (!is_string(a) || !is_string(b)) {
        // Handle non-string cases
        if (!is_string(a) && !is_string(b)) return 0; // Both non-strings are equal
        return is_string(a) ? 1 : -1; // Strings are greater than non-strings
    }
    
    // Handle tiny string comparisons - convert to null-terminated strings for proper comparison
    if (is_tiny_string(a) && is_tiny_string(b)) {
        char tiny_buffer_a[TINY_STRING_MAX_LEN + 1];
        char tiny_buffer_b[TINY_STRING_MAX_LEN + 1];
        
        const char* str_a = get_string_data_nullterm(&a, tiny_buffer_a);
        const char* str_b = get_string_data_nullterm(&b, tiny_buffer_b);
        
        return strcmp(str_a, str_b);
    }
    
    // Handle mixed tiny/heap string comparisons
    if (is_tiny_string(a) || is_tiny_string(b)) {
        // Convert to C strings for comparison
        char tiny_buffer_a[TINY_STRING_MAX_LEN + 1];
        char tiny_buffer_b[TINY_STRING_MAX_LEN + 1];
        
        const char* str_a = get_string_data_nullterm(&a, tiny_buffer_a);
        const char* str_b = get_string_data_nullterm(&b, tiny_buffer_b);
        
        return strcmp(str_a, str_b);
    }
    
    // Both are heap strings - use StringStorage comparison
    StringStorage* storage_a = as_string(a);
    StringStorage* storage_b = as_string(b);
    
    if (storage_a == NULL || storage_b == NULL) {
        if (storage_a == storage_b) return 0;
        return storage_a == NULL ? -1 : 1;
    }
    
    return ss_compare(storage_a, storage_b);
}

// Helper function to print a string with escape sequences (for debugging)
// Accessor functions for debug output (used by gc_debug_output.c)
void* value_string_get_intern_table(void) {
    return intern_table;
}

void* value_string_get_intern_entry_at(int bucket) {
    if (bucket < 0 || bucket >= INTERN_TABLE_SIZE) return NULL;
    return intern_table[bucket];
}

void* value_string_get_next_intern_entry(void* entry) {
    if (!entry) return NULL;
    return ((InternEntry*)entry)->next;
}

Value value_string_get_entry_value(void* entry) {
    if (!entry) return make_null();
    return ((InternEntry*)entry)->string_value;
}

int value_string_get_intern_table_size(void) {
    return INTERN_TABLE_SIZE;
}

bool value_string_is_intern_table_initialized(void) {
    return intern_table_initialized;
}
