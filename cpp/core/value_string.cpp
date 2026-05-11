// value_string.cpp — string operations on NaN-boxed Values, layered over
// the shared StringStorage / ss_* primitives so behaviour matches CS_String.
//
// Heap Value strings are GCManager.Strings slots, each owning a
// StringStorage*. Tiny strings (≤5 ASCII bytes) live encoded in the Value
// bits and are materialised into a temporary StringStorage on demand for
// ops that need one.

#include "value_string.h"
#include "value_list.h"
#include "GCManager.h"
#include "StringStorage.h"
#include "unicodeUtil.h"
#include "hashing.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

using MiniScript::GCManager;
using MiniScript::GCString;
using MiniScript::Value;

namespace {

// Materialise a Value as a StringStorage*, possibly allocating a fresh
// temporary for tiny strings. The owned flag tracks whether the caller
// must free the storage; heap strings borrow without ownership.
class TempStorage {
    StringStorage* _ss = nullptr;
    bool _owned = false;
public:
    explicit TempStorage(Value v) {
        if (is_tiny_string(v)) {
            int len = (int)(v & 0xFF);
            if (len > 0) {
                _ss = ss_createWithLength(len, std::malloc);
                if (_ss) {
                    for (int i = 0; i < len; i++)
                        _ss->data[i] = (char)((v >> (8 * (i + 1))) & 0xFF);
                }
                _owned = true;
            }
            // len == 0 → leave _ss == nullptr (StringStorage's empty-string convention)
        } else if (is_heap_string(v)) {
            _ss = GCManager::Instance().Strings.Get(value_item_index(v)).Storage;
            _owned = false;
        }
    }
    ~TempStorage() { if (_owned && _ss) std::free(_ss); }
    TempStorage(const TempStorage&) = delete;
    TempStorage& operator=(const TempStorage&) = delete;

    StringStorage* get() const { return _ss; }
    operator StringStorage*() const { return _ss; }
    operator const StringStorage*() const { return _ss; }
};

// Wrap an ss_* result (possibly NULL for empty) into a Value, transferring
// ownership of the StringStorage to the GC manager (or freeing if interned).
Value wrap_storage(StringStorage* storage) {
    return GCManager::Instance().AdoptString(storage);
}

} // namespace

extern "C" {

// ── Creation ────────────────────────────────────────────────────────────

Value make_tiny_string(const char* str, int len) {
    if (len > TINY_STRING_MAX_LEN) len = TINY_STRING_MAX_LEN;
    Value v = TINY_STRING_TAG | (uint64_t)(uint8_t)len;
    for (int i = 0; i < len; i++)
        v |= (uint64_t)(uint8_t)str[i] << (8 * (i + 1));
    return v;
}

Value make_string(const char* str) {
    if (str == nullptr) return val_null;
    int lenB = (int)std::strlen(str);
    return GCManager::Instance().NewString(str, lenB);
}

Value make_string_n(const char* str, int len) {
    if (str == nullptr || len < 0) return val_null;
    return GCManager::Instance().NewString(str, len);
}

// ── Access ──────────────────────────────────────────────────────────────

const char* as_cstring(Value v) {
    static thread_local char tiny_scratch[TINY_STRING_MAX_LEN + 1];
    if (is_tiny_string(v)) {
        int len = (int)(v & 0xFF);
        for (int i = 0; i < len; i++)
            tiny_scratch[i] = (char)((v >> (8 * (i + 1))) & 0xFF);
        tiny_scratch[len] = '\0';
        return tiny_scratch;
    }
    if (is_heap_string(v)) {
        const GCString& s = GCManager::Instance().Strings.Get(value_item_index(v));
        return ss_getCString(s.Storage);
    }
    return "";
}

int string_lengthB(Value v) {
    if (is_tiny_string(v)) return (int)(v & 0xFF);
    if (is_heap_string(v))
        return ss_lengthB(GCManager::Instance().Strings.Get(value_item_index(v)).Storage);
    return 0;
}

int string_length(Value v) {
    if (is_tiny_string(v)) {
        int lenB = (int)(v & 0xFF);
        if (lenB == 0) return 0;
        char buf[TINY_STRING_MAX_LEN + 1];
        for (int i = 0; i < lenB; i++)
            buf[i] = (char)((v >> (8 * (i + 1))) & 0xFF);
        return UTF8CharacterCount((const unsigned char*)buf, lenB);
    }
    if (is_heap_string(v))
        return ss_lengthC(GCManager::Instance().Strings.Get(value_item_index(v)).Storage);
    return 0;
}

const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len) {
    Value v = *v_ptr;
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(v_ptr);
        *out_len = (int)(unsigned char)data[0];
        return data + 1;
    }
    if (is_heap_string(v)) {
        const GCString& s = GCManager::Instance().Strings.Get(value_item_index(v));
        *out_len = ss_lengthB(s.Storage);
        return ss_getCString(s.Storage);
    }
    *out_len = 0;
    return nullptr;
}

const char* get_string_data_nullterm(const Value* v_ptr, char* tiny_buffer) {
    Value v = *v_ptr;
    if (is_tiny_string(v)) {
        const char* data = GET_VALUE_DATA_PTR_CONST(v_ptr);
        int len = (int)(unsigned char)data[0];
        if (len < TINY_STRING_MAX_LEN) return data + 1;
        for (int i = 0; i < len; i++) tiny_buffer[i] = data[1 + i];
        tiny_buffer[len] = '\0';
        return tiny_buffer;
    }
    if (is_heap_string(v))
        return ss_getCString(GCManager::Instance().Strings.Get(value_item_index(v)).Storage);
    return nullptr;
}

// ── Operations ──────────────────────────────────────────────────────────

bool string_equals(Value a, Value b) {
    if (!is_string(a) || !is_string(b)) return false;
    if (a == b) return true;  // identical bits → equal (interning makes this common)
    if (is_tiny_string(a) && is_tiny_string(b)) return false;
    TempStorage ta(a), tb(b);
    return ss_equals(ta, tb);
}

int string_compare(Value a, Value b) {
    TempStorage ta(a), tb(b);
    return ss_compare(ta, tb);
}

Value string_concat(Value a, Value b) {
    // Bypass ss_concat when either input is empty: ss_concat returns the
    // other input's storage by reference in that case, which would alias a
    // pointer already owned elsewhere (a GCString slot or our TempStorage)
    // and lead to double-free.
    if (string_lengthB(a) == 0) return b;
    if (string_lengthB(b) == 0) return a;
    TempStorage ta(a), tb(b);
    StringStorage* result = ss_concat(ta, tb, std::malloc);
    return wrap_storage(result);
}

int string_indexOf(Value haystack, Value needle, int start_pos) {
    TempStorage th(haystack), tn(needle);
    if (start_pos <= 0) return ss_indexOf(th, tn);
    return ss_indexOfFrom(th, tn, start_pos);
}

Value string_substring(Value str, int startIndex, int len) {
    TempStorage ts(str);
    if (startIndex < 0) startIndex += ss_lengthC(ts);
    StringStorage* result = ss_substringLen(ts, startIndex, len, std::malloc);
    return wrap_storage(result);
}

Value string_slice(Value str, int start, int end) {
    TempStorage ts(str);
    int slen = ss_lengthC(ts);
    if (start < 0) start += slen;
    if (end   < 0) end   += slen;
    if (start < 0) start = 0;
    if (end > slen) end = slen;
    if (start >= end) return val_empty_string;
    StringStorage* result = ss_substringLen(ts, start, end - start, std::malloc);
    return wrap_storage(result);
}

Value string_sub(Value a, Value b) {
    if (!is_string(a) || !is_string(b)) return val_null;
    TempStorage ta(a), tb(b);
    if (!tb.get() || ss_lengthB(tb) == 0) return a;
    if (ss_endsWith(ta, tb)) {
        int newLen = ss_lengthB(ta) - ss_lengthB(tb);
        StringStorage* result = ss_substringLen(ta, 0, newLen, std::malloc);
        return wrap_storage(result);
    }
    return a;
}

Value string_insert(Value str, int index, Value value, void* vm) {
    if (!is_string(str)) return str;
    Value insertVal = is_string(value) ? value : to_string(value, vm);
    int strLenB    = string_lengthB(str);
    int strLenC    = string_length(str);
    int insertLenB = string_lengthB(insertVal);

    if (index < 0) index += strLenC + 1;
    if (index < 0) index = 0;
    if (index > strLenC) index = strLenC;

    if (insertLenB == 0) return str;

    char tinyBufS[TINY_STRING_MAX_LEN + 1];
    char tinyBufI[TINY_STRING_MAX_LEN + 1];
    const char* sData = get_string_data_nullterm(&str, tinyBufS);
    const char* iData = get_string_data_nullterm(&insertVal, tinyBufI);
    if (!sData || !iData) return str;

    int byteIdx = UTF8CharIndexToByteIndex((const unsigned char*)sData, index, strLenB);
    if (byteIdx < 0) byteIdx = strLenB;

    int totalLenB = strLenB + insertLenB;
    char* buf = (char*)std::malloc((size_t)(totalLenB + 1));
    std::memcpy(buf, sData, (size_t)byteIdx);
    std::memcpy(buf + byteIdx, iData, (size_t)insertLenB);
    std::memcpy(buf + byteIdx + insertLenB, sData + byteIdx, (size_t)(strLenB - byteIdx));
    buf[totalLenB] = '\0';

    Value result = GCManager::Instance().NewString(buf, totalLenB);
    std::free(buf);
    return result;
}

Value string_upper(Value str) {
    if (!is_string(str)) return str;
    TempStorage ts(str);
    StringStorage* result = ss_toUpper(ts, std::malloc);
    // ss_toUpper returns the input storage when it has no caseable chars;
    // returning `str` directly avoids aliasing the borrowed (heap) or
    // TempStorage-owned (tiny) pointer through wrap_storage.
    if (result == ts.get()) return str;
    return wrap_storage(result);
}

Value string_lower(Value str) {
    if (!is_string(str)) return str;
    TempStorage ts(str);
    StringStorage* result = ss_toLower(ts, std::malloc);
    if (result == ts.get()) return str;
    return wrap_storage(result);
}

Value string_from_code_point(int codePoint) {
    char buf[8];
    int n = 0;
    if (codePoint < 0x80) {
        buf[n++] = (char)codePoint;
    } else if (codePoint < 0x800) {
        buf[n++] = (char)(0xC0 | (codePoint >> 6));
        buf[n++] = (char)(0x80 | (codePoint & 0x3F));
    } else if (codePoint < 0x10000) {
        buf[n++] = (char)(0xE0 | (codePoint >> 12));
        buf[n++] = (char)(0x80 | ((codePoint >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (codePoint & 0x3F));
    } else {
        buf[n++] = (char)(0xF0 | (codePoint >> 18));
        buf[n++] = (char)(0x80 | ((codePoint >> 12) & 0x3F));
        buf[n++] = (char)(0x80 | ((codePoint >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (codePoint & 0x3F));
    }
    return GCManager::Instance().NewString(buf, n);
}

int string_code_point(Value str) {
    if (!is_string(str)) return 0;
    TempStorage ts(str);
    if (!ts.get() || ss_lengthB(ts) == 0) return 0;
    return (int)ss_codePointAt(ts, 0);
}

// ss_* doesn't expose split-with-limit; do byte-level work for that case
// while delegating the unbounded case to ss_splitStr.

Value string_split(Value str, Value delimiter) {
    return string_split_max(str, delimiter, -1);
}

Value string_split_max(Value str, Value delimiter, int maxCount) {
    if (!is_string(str) || !is_string(delimiter)) return val_null;
    int slen, dlen;
    const char* sdata = get_string_data_zerocopy(&str, &slen);
    const char* ddata = get_string_data_zerocopy(&delimiter, &dlen);

    Value list = make_list(8);

    if (dlen == 0) {
        // Empty delimiter: split into individual UTF-8 code points.
        int i = 0; int count = 0;
        while (i < slen) {
            if (maxCount > 0 && count >= maxCount - 1) {
                list_push(list, GCManager::Instance().NewString(sdata + i, slen - i));
                return list;
            }
            unsigned char c = (unsigned char)sdata[i];
            int cl = 1;
            if      ((c & 0xE0) == 0xC0) cl = 2;
            else if ((c & 0xF0) == 0xE0) cl = 3;
            else if ((c & 0xF8) == 0xF0) cl = 4;
            if (i + cl > slen) cl = slen - i;
            list_push(list, GCManager::Instance().NewString(sdata + i, cl));
            i += cl;
            count++;
        }
        return list;
    }

    int pos = 0; int found = 0;
    while (pos <= slen) {
        const char* hit = nullptr;
        if (maxCount <= 0 || found < maxCount - 1) {
            for (int i = pos; i + dlen <= slen; i++) {
                if (std::memcmp(sdata + i, ddata, (size_t)dlen) == 0) {
                    hit = sdata + i;
                    break;
                }
            }
        }
        if (!hit) {
            list_push(list, GCManager::Instance().NewString(sdata + pos, slen - pos));
            break;
        }
        int segLen = (int)(hit - (sdata + pos));
        list_push(list, GCManager::Instance().NewString(sdata + pos, segLen));
        pos += segLen + dlen;
        found++;
        if (pos > slen) break;
        if (pos == slen) { list_push(list, val_empty_string); break; }
    }
    return list;
}

Value string_replace(Value source, Value search, Value replacement) {
    if (!is_string(source) || !is_string(search) || !is_string(replacement))
        return val_null;
    TempStorage ts(source), tf(search), tr(replacement);
    if (!tf.get() || ss_lengthB(tf) == 0) return source;
    StringStorage* result = ss_replace(ts, tf, tr, std::malloc);
    return wrap_storage(result);
}

Value string_replace_max(Value source, Value search, Value replacement, int maxCount) {
    if (maxCount <= 0) return string_replace(source, search, replacement);
    if (!is_string(source) || !is_string(search) || !is_string(replacement))
        return val_null;
    int slen, flen, rlen;
    const char* sdata = get_string_data_zerocopy(&source, &slen);
    const char* fdata = get_string_data_zerocopy(&search, &flen);
    const char* rdata = get_string_data_zerocopy(&replacement, &rlen);
    if (flen == 0) return source;

    int matches = 0;
    for (int i = 0; i + flen <= slen; ) {
        if (std::memcmp(sdata + i, fdata, (size_t)flen) == 0) {
            matches++;
            if (matches >= maxCount) break;
            i += flen;
        } else i++;
    }
    if (matches == 0) return source;
    int outLen = slen + matches * (rlen - flen);
    char* buf = (char*)std::malloc((size_t)(outLen > 0 ? outLen : 1));
    int read = 0, write = 0, replaced = 0;
    while (read < slen) {
        bool willReplace = (read + flen <= slen)
            && (replaced < maxCount)
            && std::memcmp(sdata + read, fdata, (size_t)flen) == 0;
        if (willReplace) {
            std::memcpy(buf + write, rdata, (size_t)rlen);
            write += rlen; read += flen; replaced++;
        } else {
            buf[write++] = sdata[read++];
        }
    }
    Value result = GCManager::Instance().NewString(buf, outLen);
    std::free(buf);
    return result;
}

// ── Hashing ─────────────────────────────────────────────────────────────
// string_hash(const char*, int) lives in hashing.c.

uint32_t get_string_hash(Value v) {
    if (is_tiny_string(v)) {
        char buf[TINY_STRING_MAX_LEN + 1];
        int len = (int)(v & 0xFF);
        for (int i = 0; i < len; i++)
            buf[i] = (char)((v >> (8 * (i + 1))) & 0xFF);
        return string_hash(buf, len);
    }
    if (is_heap_string(v)) {
        GCString& s = GCManager::Instance().Strings.Get(value_item_index(v));
        return ss_hash(s.Storage);
    }
    return 0;
}

} // extern "C"
