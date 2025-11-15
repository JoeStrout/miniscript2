#include "StringStorage.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "unicodeUtil.h"
#include "hashing.h"

#include "layer_defs.h"
#if LAYER_1_VIOLATIONS
#error "StringStorage.h (Layer 1) cannot depend on higher layers"
#endif

// Creation and destruction functions
StringStorage* ss_create(const char* cstr, StringStorageAllocator allocator) {
    if (!cstr || !allocator) return NULL;
    
    int len = strlen(cstr);
    
    // Optimization: empty strings are represented as NULL
    if (len == 0) return NULL;
    
    StringStorage* storage = (StringStorage*)allocator(sizeof(StringStorage) + len + 1);
    if (!storage) return NULL;
    
    storage->lenB = len;
    storage->lenC = -1;  // Compute lazily when needed
    storage->hash = 0;   // Compute lazily when needed
    strcpy(storage->data, cstr);
    
    return storage;
}

StringStorage* ss_createWithLength(int byteLen, StringStorageAllocator allocator) {
    if (byteLen < 0 || !allocator) return NULL;
    
    // Optimization: empty strings are represented as NULL
    if (byteLen == 0) return NULL;
    
    StringStorage* storage = (StringStorage*)allocator(sizeof(StringStorage) + byteLen + 1);
    if (!storage) return NULL;
    
    storage->lenB = byteLen;
    storage->lenC = -1;  // Will be computed when needed
    storage->hash = 0;   // Will be computed when needed
    storage->data[byteLen] = '\0';  // Ensure null termination
    
    return storage;
}

// Basic accessor functions
const char* ss_getCString(const StringStorage* storage) {
    return storage ? storage->data : "";
}

int ss_lengthB(const StringStorage* storage) {
    return storage ? storage->lenB : 0;
}

int ss_lengthC(const StringStorage* storage) {
    if (!storage) return 0;
    
    // If character count hasn't been computed yet, compute and cache it
    if (storage->lenC < 0) {
        // We need to cast away const to cache the result
        StringStorage* mutable_storage = (StringStorage*)storage;
        mutable_storage->lenC = UTF8CharacterCount(
          (const unsigned char*)storage->data, storage->lenB);
    }
    
    return storage->lenC;
}

bool ss_isEmpty(const StringStorage* storage) {
    return !storage || storage->lenB == 0;
}

char ss_charAt(const StringStorage* storage, int byteIndex) {
    if (!storage || byteIndex < 0 || byteIndex >= storage->lenB) return '\0';
    return storage->data[byteIndex];
}

// Comparison functions
bool ss_equals(const StringStorage* storage, const StringStorage* other) {
    // NULL represents empty string, so NULL == NULL is true
    if (!storage && !other) return true;
    if (!storage || !other) return false;
    if (storage == other) return true;
    if (storage->lenB != other->lenB) return false;
    return memcmp(storage->data, other->data, storage->lenB) == 0;
}

int ss_compare(const StringStorage* storage, const StringStorage* other) {
    if (!storage) return !other ? 0 : -1;
    if (!other) return 1;
    if (storage == other) return 0;
    
    int result = memcmp(storage->data, other->data, storage->lenB < other->lenB ? storage->lenB : other->lenB);
    if (result == 0) {
        return storage->lenB - other->lenB;
    }
    return result;
}

// Search functions
int ss_indexOf(const StringStorage* storage, const StringStorage* needle) {
    return ss_indexOfFrom(storage, needle, 0);
}

int ss_indexOfFrom(const StringStorage* storage, const StringStorage* needle, int startIndex) {
    if (!storage || !needle) return -1;
    if (startIndex < 0 || startIndex >= ss_lengthC(storage)) return -1;
    if (ss_isEmpty(needle)) return startIndex;
    
    // Convert character index to byte index
    int startByteIndex = UTF8CharIndexToByteIndex(
        (const unsigned char*)storage->data, startIndex, storage->lenB);
    if (startByteIndex < 0) return -1;
    
    const char* found = strstr(storage->data + startByteIndex, needle->data);
    if (!found) return -1;
    
    // Convert back to character index
    int foundByteIndex = found - storage->data;
    return UTF8ByteIndexToCharIndex(
        (const unsigned char*)storage->data, foundByteIndex, storage->lenB);
}

int ss_indexOfChar(const StringStorage* storage, char ch) {
    return ss_indexOfCharFrom(storage, ch, 0);
}

int ss_indexOfCharFrom(const StringStorage* storage, char ch, int startIndex) {
    if (!storage) return -1;
    if (startIndex < 0 || startIndex >= ss_lengthC(storage)) return -1;
    
    // Convert character index to byte index
    int startByteIndex = UTF8CharIndexToByteIndex(
        (const unsigned char*)storage->data, startIndex, storage->lenB);
    if (startByteIndex < 0) return -1;
    
    const char* found = strchr(storage->data + startByteIndex, ch);
    if (!found) return -1;
    
    // Convert back to character index
    int foundByteIndex = found - storage->data;
    return UTF8ByteIndexToCharIndex(
        (const unsigned char*)storage->data, foundByteIndex, storage->lenB);
}

int ss_lastIndexOf(const StringStorage* storage, const StringStorage* needle) {
    if (!storage || !needle || ss_isEmpty(needle)) return -1;
    if (needle->lenB > storage->lenB) return -1;
    
    // Search backwards
    for (int i = storage->lenB - needle->lenB; i >= 0; i--) {
        if (memcmp(storage->data + i, needle->data, needle->lenB) == 0) {
            return UTF8ByteIndexToCharIndex((const unsigned char*)storage->data, i, storage->lenB);
        }
    }
    return -1;
}

int ss_lastIndexOfChar(const StringStorage* storage, char ch) {
    if (!storage) return -1;
    const char* found = strrchr(storage->data, ch);
    if (!found) return -1;
    
    int foundByteIndex = found - storage->data;
    return UTF8ByteIndexToCharIndex((const unsigned char*)storage->data, foundByteIndex, storage->lenB);
}

bool ss_contains(const StringStorage* storage, const StringStorage* needle) {
    return ss_indexOf(storage, needle) >= 0;
}

bool ss_startsWith(const StringStorage* storage, const StringStorage* prefix) {
    if (!storage || !prefix) return false;
    if (prefix->lenB > storage->lenB) return false;
    return memcmp(storage->data, prefix->data, prefix->lenB) == 0;
}

bool ss_endsWith(const StringStorage* storage, const StringStorage* suffix) {
    if (!storage || !suffix) return false;
    if (suffix->lenB > storage->lenB) return false;
    return memcmp(storage->data + storage->lenB - suffix->lenB, suffix->data, suffix->lenB) == 0;
}

// String manipulation functions
StringStorage* ss_substring(const StringStorage* storage, int startIndex, StringStorageAllocator allocator) {
    return ss_substringLen(storage, startIndex, ss_lengthC(storage) - startIndex, allocator);
}

StringStorage* ss_substringLen(const StringStorage* storage, int startIndex, int length, StringStorageAllocator allocator) {
    if (!storage || startIndex < 0 || length < 0 || !allocator) return NULL;
    if (startIndex >= ss_lengthC(storage)) return ss_create("", allocator);
    
    // Convert character indices to byte indices
    int startByteIndex = UTF8CharIndexToByteIndex(
        (const unsigned char*)storage->data, startIndex, storage->lenB);
    if (startByteIndex < 0) return ss_create("", allocator);
    
    int endCharIndex = startIndex + length;
    if (endCharIndex > ss_lengthC(storage)) endCharIndex = ss_lengthC(storage);
    int endByteIndex = UTF8CharIndexToByteIndex(
        (const unsigned char*)storage->data, endCharIndex, storage->lenB);
    if (endByteIndex < 0) endByteIndex = storage->lenB;
    
    int subLenB = endByteIndex - startByteIndex;
    if (subLenB <= 0) return ss_create("", allocator);
    
    StringStorage* result = ss_createWithLength(subLenB, allocator);
    if (!result) return NULL;
    
    memcpy(result->data, storage->data + startByteIndex, subLenB);
    result->lenC = endCharIndex - startIndex;  // We know the exact character count!
    
    return result;
}

StringStorage* ss_concat(const StringStorage* storage, const StringStorage* other, StringStorageAllocator allocator) {
    if (!allocator) return NULL;
    
    // Handle empty strings (NULL means empty) - strings are immutable, so just return the non-empty one
    if (!storage && !other) return NULL;  // empty + empty = empty
    if (!storage) return (StringStorage*)other;  // empty + other = other
    if (!other) return (StringStorage*)storage;   // storage + empty = storage
    
    int totalLen = storage->lenB + other->lenB;
    StringStorage* result = ss_createWithLength(totalLen, allocator);
    if (!result) return NULL;
    
    memcpy(result->data, storage->data, storage->lenB);
    memcpy(result->data + storage->lenB, other->data, other->lenB);
    result->lenC = ss_lengthC(storage) + ss_lengthC(other);
    
    return result;
}

StringStorage* ss_toLower(const StringStorage* storage, StringStorageAllocator allocator) {
    if (!storage || !allocator) return NULL;
    
    // Check if string is caseless (immutable optimization)
    if (UTF8IsCaseless((unsigned char*)storage->data, storage->lenB)) {
    	return (StringStorage*)storage;  // No caseable strings.
    }
    
    // Otherwise, do the conversion into a new buffer, and then copy it
    // (OFI: add plumbing to avoid the extra copy).
    unsigned char *newBuf;
    unsigned long newLenB;
    if (!UTF8ToLower((unsigned char*)storage->data, storage->lenB, &newBuf, &newLenB)) {
    	// Nothing actually changed.  Return same string.
    	free(newBuf);
    	return (StringStorage*)storage;
    }

    StringStorage* result = ss_createWithLength(newLenB, allocator);
    if (!result) return NULL;
    memcpy(result->data, newBuf, (size_t)newLenB);
    result->lenB = newLenB;
    result->lenC = storage->lenC;  // (assume same number of characters)
    
    return result;
}

StringStorage* ss_toUpper(const StringStorage* storage, StringStorageAllocator allocator) {
    if (!storage || !allocator) return NULL;
    
    // Check if string is caseless (immutable optimization)
    if (UTF8IsCaseless((unsigned char*)storage->data, storage->lenB)) {
    	return (StringStorage*)storage;  // No caseable strings.
    }
    
    // Otherwise, do the conversion into a new buffer, and then copy it
    // (OFI: add plumbing to avoid the extra copy).
    unsigned char *newBuf;
    unsigned long newLenB;
    if (!UTF8ToUpper((unsigned char*)storage->data, storage->lenB, &newBuf, &newLenB)) {
    	// Nothing actually changed.  Return same string.
    	free(newBuf);
    	return (StringStorage*)storage;
    }

    StringStorage* result = ss_createWithLength(newLenB, allocator);
    if (!result) return NULL;
    memcpy(result->data, newBuf, (size_t)newLenB);
    result->lenB = newLenB;
    result->lenC = storage->lenC;  // (assume same number of characters)
    
    return result;
}

StringStorage* ss_trim(const StringStorage* storage, StringStorageAllocator allocator) {
    if (!storage || ss_isEmpty(storage)) return ss_create("", allocator);
    if (!allocator) return NULL;
    
    unsigned char* start = (unsigned char*)storage->data;
    unsigned char* end = start + storage->lenB;
    
    // Find first non-whitespace character
    while (start < end) {
        unsigned char* next = start;
        unsigned long codePoint = UTF8DecodeAndAdvance(&next);
        if (!UnicodeCharIsWhitespace(codePoint)) break;
        start = next;
    }
    
    // Find last non-whitespace character
    unsigned char* lastNonWhite = start;
    unsigned char* current = start;
    while (current < end) {
        unsigned char* next = current;
        unsigned long codePoint = UTF8DecodeAndAdvance(&next);
        if (!UnicodeCharIsWhitespace(codePoint)) {
            lastNonWhite = next;
        }
        current = next;
    }
    
    if (start >= lastNonWhite) return ss_create("", allocator);
    
    int trimmedLen = lastNonWhite - start;
    StringStorage* result = ss_createWithLength(trimmedLen, allocator);
    if (!result) return NULL;
    
    memcpy(result->data, start, trimmedLen);
    
    return result;
}

StringStorage* ss_trimStart(const StringStorage* storage, StringStorageAllocator allocator) {
    if (!storage || ss_isEmpty(storage)) return ss_create("", allocator);
    if (!allocator) return NULL;
    
    unsigned char* start = (unsigned char*)storage->data;
    unsigned char* end = start + storage->lenB;
    
    // Find first non-whitespace character
    while (start < end) {
        unsigned char* next = start;
        unsigned long codePoint = UTF8DecodeAndAdvance(&next);
        if (!UnicodeCharIsWhitespace(codePoint)) break;
        start = next;
    }
    
    int trimmedLen = end - start;
    if (trimmedLen <= 0) return ss_create("", allocator);
    
    StringStorage* result = ss_createWithLength(trimmedLen, allocator);
    if (!result) return NULL;
    
    memcpy(result->data, start, trimmedLen);
    
    return result;
}

StringStorage* ss_trimEnd(const StringStorage* storage, StringStorageAllocator allocator) {
    if (!storage || ss_isEmpty(storage)) return ss_create("", allocator);
    if (!allocator) return NULL;
    
    unsigned char* start = (unsigned char*)storage->data;
    unsigned char* end = start + storage->lenB;
    
    // Find last non-whitespace character
    unsigned char* lastNonWhite = start;
    unsigned char* current = start;
    while (current < end) {
        unsigned char* next = current;
        unsigned long codePoint = UTF8DecodeAndAdvance(&next);
        if (!UnicodeCharIsWhitespace(codePoint)) {
            lastNonWhite = next;
        }
        current = next;
    }
    
    int trimmedLen = lastNonWhite - start;
    if (trimmedLen <= 0) return ss_create("", allocator);
    
    StringStorage* result = ss_createWithLength(trimmedLen, allocator);
    if (!result) return NULL;
    
    memcpy(result->data, start, trimmedLen);
    
    return result;
}

bool ss_isNullOrWhiteSpace(const StringStorage* storage) {
    if (!storage || ss_isEmpty(storage)) return true;
    
    unsigned char* ptr = (unsigned char*)storage->data;
    unsigned char* end = ptr + storage->lenB;
    
    while (ptr < end) {
        unsigned long codePoint = UTF8DecodeAndAdvance(&ptr);
        if (!UnicodeCharIsWhitespace(codePoint)) {
            return false;
        }
    }
    return true;
}

StringStorage** ss_split(const StringStorage* storage, char separator, int* count, StringStorageAllocator allocator) {
    *count = 0;
    if (!allocator) return NULL;
    
    if (!storage || ss_isEmpty(storage)) {
        StringStorage** result = (StringStorage**)malloc(sizeof(StringStorage*));
        result[0] = ss_create("", allocator);
        *count = 1;
        return result;
    }
    
    // Count separators
    int sepCount = 0;
    for (int i = 0; i < storage->lenB; i++) {
        if (storage->data[i] == separator) sepCount++;
    }
    
    // Allocate array for results
    int resultCount = sepCount + 1;
    StringStorage** result = (StringStorage**)malloc(resultCount * sizeof(StringStorage*));
    
    // Split the string
    int resultIndex = 0;
    int start = 0;
    
    for (int i = 0; i <= storage->lenB; i++) {
        if (i == storage->lenB || storage->data[i] == separator) {
            int tokenLen = i - start;
            StringStorage* token = ss_createWithLength(tokenLen, allocator);
            if (token) {
                memcpy(token->data, storage->data + start, tokenLen);
            }
            result[resultIndex] = token;
            resultIndex++;
            start = i + 1;
        }
    }
    
    *count = resultCount;
    return result;
}

uint32_t ss_computeHash(const StringStorage* storage) {
    if (!storage) return 0;
    return string_hash(storage->data, storage->lenB);
}

// Not yet implemented - more complex methods
StringStorage* ss_insert(const StringStorage* storage, int startIndex, const StringStorage* value, StringStorageAllocator allocator) {
    // TODO: Implement
    (void)storage; (void)startIndex; (void)value; (void)allocator;
    return NULL;
}

StringStorage* ss_remove(const StringStorage* storage, int startIndex, StringStorageAllocator allocator) {
    // TODO: Implement
    (void)storage; (void)startIndex; (void)allocator;
    return NULL;
}

StringStorage* ss_removeLen(const StringStorage* storage, int startIndex, int count, StringStorageAllocator allocator) {
    // TODO: Implement
    (void)storage; (void)startIndex; (void)count; (void)allocator;
    return NULL;
}

StringStorage* ss_replace(const StringStorage* storage, const StringStorage* oldValue, const StringStorage* newValue, StringStorageAllocator allocator) {
    if (!storage || !oldValue || !newValue) return NULL;

    const char* str = storage->data;
    const char* old_str = oldValue->data;
    const char* new_str = newValue->data;

    int str_len = storage->lenB;
    int old_len = oldValue->lenB;
    int new_len = newValue->lenB;

    // If old string is empty, return copy of original
    if (old_len == 0) {
        return ss_create(storage->data, allocator);
    }

    // Count occurrences of old_str in str
    int count = 0;
    const char* pos = str;
    while ((pos = strstr(pos, old_str)) != NULL) {
        count++;
        pos += old_len;
    }

    // If no occurrences, return copy of original
    if (count == 0) {
        return ss_create(storage->data, allocator);
    }

    // Calculate new length
    int new_total_len = str_len + count * (new_len - old_len);

    // Allocate new StringStorage
    StringStorage* result = (StringStorage*)allocator(sizeof(StringStorage) + new_total_len + 1);
    if (!result) return NULL;

    result->lenB = new_total_len;
    result->lenC = -1; // Will be computed on demand
    result->hash = 0;  // Will be computed on demand

    // Build the result string
    char* dest = result->data;
    const char* src = str;

    while (*src) {
        const char* found = strstr(src, old_str);
        if (found == src) {
            // Found occurrence at current position
            memcpy(dest, new_str, new_len);
            dest += new_len;
            src += old_len;
        } else {
            // Copy characters until next occurrence or end
            const char* next = found ? found : str + str_len;
            int copy_len = next - src;
            memcpy(dest, src, copy_len);
            dest += copy_len;
            src += copy_len;
        }
    }

    *dest = '\0';
    return result;
}

StringStorage* ss_replaceByte(const StringStorage* storage, char oldChar, char newChar, StringStorageAllocator allocator) {
    if (!storage) return NULL;

    // No sense in trying to replace '\0'. Return a copy of original.
    if (oldChar == '\0') {
        return ss_create(storage->data, allocator);
    }

    // Allocate new StringStorage
    StringStorage* result = (StringStorage*)allocator(sizeof(StringStorage) + storage->lenB + 1);
    if (!result) return NULL;

    result->lenB = storage->lenB;
    result->lenC = -1; // Will be computed on demand
    result->hash = 0;  // Will be computed on demand

    // Build the result string
    char* dest = result->data;
    const char* src = storage->data;

    while (*src) { // We'll do it manually
        *dest = *src == oldChar ? newChar : *src;
    }

    *dest = '\0';
    return result;
}

StringStorage** ss_splitStr(const StringStorage* storage, const StringStorage* separator, int* count, StringStorageAllocator allocator) {
    // TODO: Implement
    (void)storage; (void)separator; (void)count; (void)allocator;
    return NULL;
}
