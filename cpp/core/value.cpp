// value.cpp — runtime utilities, error/funcref accessors, code_form, etc.
//
// Most logic is unchanged from the legacy value.c. Error and FuncRef
// accessors now dispatch through GCManager instead of pointer decoding.

#include "value.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include "vm_error.h"
#include "GCManager.g.h"
#include "hashing.h"

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "value.cpp (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>

using MiniScript::GCManager;
using MiniScript::GCList;
using MiniScript::GCMap;
using MiniScript::GCError;
using MiniScript::GCFunction;

extern "C" {

// ── Constants ───────────────────────────────────────────────────────────

Value val_isa_key = 0;
Value val_self    = 0;
Value val_super   = 0;

void value_init_constants(void) {
    val_isa_key = make_string("__isa");
    val_self    = make_string("self");
    val_super   = make_string("super");
}

// ── Error accessors ─────────────────────────────────────────────────────

Value make_error(Value message, Value inner, Value stack, Value isa) {
    if (is_null(stack)) {
        stack = make_list(0);
        freeze_value(stack);
    }
    return GCManager::NewError(message, inner, stack, isa);
}

Value error_message(Value v) {
    if (!is_error(v)) return val_null;
    return GCManager::Errors.Get(value_item_index(v)).Message;
}

Value error_inner(Value v) {
    if (!is_error(v)) return val_null;
    return GCManager::Errors.Get(value_item_index(v)).Inner;
}

Value error_stack(Value v) {
    if (!is_error(v)) return val_null;
    return GCManager::Errors.Get(value_item_index(v)).Stack;
}

Value error_isa(Value v) {
    if (!is_error(v)) return val_null;
    return GCManager::Errors.Get(value_item_index(v)).Isa;
}

bool error_isa_contains(Value error, Value base) {
    Value current = error;
    for (int depth = 0; depth < 256; depth++) {
        if (is_null(current)) return false;
        if (value_identical(current, base)) return true;
        if (!is_error(current)) return false;
        current = GCManager::Errors.Get(value_item_index(current)).Isa;
    }
    return false;
}

// ── FuncRef accessors ───────────────────────────────────────────────────

}  // end extern "C": make_funcref/funcref_funcdef use the C++ type FuncDef

Value make_funcref(MiniScript::FuncDef func, Value outerVars) {
    return GCManager::NewFuncRef(func, outerVars);
}

MiniScript::FuncDef funcref_funcdef(Value v) {
    if (!is_funcref(v)) return MiniScript::FuncDef();
    return GCManager::Functions.Get(value_item_index(v)).Func;
}

extern "C" {

Value funcref_outer_vars(Value v) {
    if (!is_funcref(v)) return val_null;
    return GCManager::Functions.Get(value_item_index(v)).OuterVars;
}

// ── Arithmetic helpers ──────────────────────────────────────────────────

Value value_mult_nonnumeric(Value a, Value b) {
    if (is_string(a) && is_double(b)) {
        double factor = as_double(b);
        int factorClass = std::fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return val_null;
        if (factor <= 0) return val_empty_string;
        if (string_length(a) * factor > MAX_COLLECTION_SIZE) {
            return value_make_runtime_error("string too large (exceeds maximum size)");
        }
        int repeats = (int)factor;
        Value result = val_empty_string;
        for (int i = 0; i < repeats; i++) result = string_concat(result, a);
        int extraChars = (int)(string_length(a) * (factor - repeats));
        if (extraChars > 0) result = string_concat(result, string_substring(a, 0, extraChars));
        return result;
    }
    if (is_list(a) && is_double(b)) {
        double factor = as_double(b);
        int factorClass = std::fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return val_null;
        int len = list_count(a);
        if (factor <= 0 || len == 0) return make_list(0);
        if (len * factor > MAX_COLLECTION_SIZE) {
            return value_make_runtime_error("list too large (exceeds maximum size)");
        }
        int fullCopies = (int)factor;
        int extraItems = (int)(len * (factor - fullCopies));
        Value result = make_list(fullCopies * len + extraItems);
        for (int c = 0; c < fullCopies; c++)
            for (int i = 0; i < len; i++) list_push(result, list_get(a, i));
        for (int i = 0; i < extraItems; i++) list_push(result, list_get(a, i));
        return result;
    }
    return val_null;
}

bool value_le(Value a, Value b) {
    if (is_double(a) && is_double(b)) return as_double(a) <= as_double(b);
    if (is_string(a) && is_string(b)) return string_compare(a, b) <= 0;
    return false;
}

bool value_equal(Value a, Value b) {
    if (a == b) return true;
    if (is_double(a) && is_double(b)) return as_double(a) == as_double(b);
    if (is_string(a) && is_string(b)) return string_equals(a, b);
    if (is_null(a)   && is_null(b))   return true;
    if (is_list(a)   && is_list(b)) {
        int n = list_count(a);
        if (n != list_count(b)) return false;
        for (int i = 0; i < n; i++)
            if (!value_equal(list_get(a, i), list_get(b, i))) return false;
        return true;
    }
    if (is_map(a) && is_map(b)) {
        GCMap mA = GCManager::Maps.Get(value_item_index(a));
        GCMap mB = GCManager::Maps.Get(value_item_index(b));
        if (mA.Count() != mB.Count()) return false;
        for (int i = mA.NextEntry(-1); i != -1; i = mA.NextEntry(i)) {
            Value bv;
            if (!mB.TryGet(mA.KeyAt(i), &bv)) return false;
            if (!value_equal(mA.ValueAt(i), bv)) return false;
        }
        return true;
    }
    // Same-type non-container reference values: identity.
    if ((a & NANISH_MASK) == (b & NANISH_MASK)) return a == b;
    return false;
}

int value_compare(Value a, Value b) {
    if (is_number(a) && is_number(b)) {
        double da = numeric_val(a), db = numeric_val(b);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
    if (is_string(a) && is_string(b)) return string_compare(a, b);
    int ta = is_number(a) ? 0 : is_string(a) ? 1 : 2;
    int tb = is_number(b) ? 0 : is_string(b) ? 1 : 2;
    return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}

// ── Bitwise ops (cast to int64, operate, cast back to double) ────────────

Value value_xor(Value a, Value b) {
    return make_double((double)((int64_t)as_double(a) ^ (int64_t)as_double(b)));
}
Value value_unary(Value a) {
    return make_double((double)(~(int64_t)as_double(a)));
}
Value value_shr(Value v, int shift) {
    return make_double((double)((int64_t)as_double(v) >> shift));
}
Value value_shl(Value v, int shift) {
    return make_double((double)((int64_t)as_double(v) << shift));
}

// ── code_form / to_string / to_number ───────────────────────────────────

namespace {

ShortNameLookupFn g_short_name_lookup = nullptr;
RuntimeErrorMakerFn g_runtime_error_maker = nullptr;
StackTraceFn g_stack_trace_hook = nullptr;

Value find_short_name(void* vm, Value v) {
    if (!vm || !g_short_name_lookup) return val_null;
    return g_short_name_lookup(vm, v);
}

// Format a double as the shortest decimal string in scientific notation that
// round-trips back to the same value, with the exponent normalized to at least
// two digits (e.g. "1.7014118346046924E+30", "1.234567E-07").  This mirrors the
// C# ShortestSci helper so both runtimes produce byte-identical output.
void format_shortest_sci(double value, char* buf) {
    char tmp[40];
    std::snprintf(tmp, sizeof tmp, "%.16E", value);
    for (int sig = 1; sig <= 17; sig++) {
        char cand[40];
        std::snprintf(cand, sizeof cand, "%.*E", sig - 1, value);
        if (std::strtod(cand, nullptr) == value) { std::strcpy(tmp, cand); break; }
    }
    // Normalize exponent to at least 2 digits (e.g. E+030 → E+30, E-8 → E-08).
    char* e = std::strchr(tmp, 'E');
    if (e) {
        size_t mlen = (size_t)(e - tmp) + 2;  // mantissa + 'E' + sign
        const char* digits = e + 2;
        while (*digits == '0' && *(digits + 1) != '\0') digits++;
        char expbuf[8];
        if (std::strlen(digits) < 2) std::snprintf(expbuf, sizeof expbuf, "0%s", digits);
        else                         std::snprintf(expbuf, sizeof expbuf, "%s", digits);
        std::memcpy(buf, tmp, mlen);
        std::strcpy(buf + mlen, expbuf);
    } else {
        std::strcpy(buf, tmp);
    }
}

void format_double(double value, char* buf) {
    if (std::isnan(value)) {
        // Match the C# formatter, which renders these as "NaN"/"Inf"/"-Inf"
        // (printf would give "nan"/"inf"/"-inf").
        std::strcpy(buf, "NaN");
        return;
    }
    if (std::isinf(value)) {
        std::strcpy(buf, value < 0 ? "-Inf" : "Inf");
        return;
    }
    if (std::fmod(value, 1.0) == 0.0) {
        // Integers up to 2^53 are represented exactly, so print them in plain
        // form.  Larger whole values have lost precision, so fall through to the
        // shortest round-trip scientific form.
        if (value <= 9007199254740992.0 && value >= -9007199254740992.0) {
            std::snprintf(buf, 32, "%.0f", value);
            if (std::strcmp(buf, "-0") == 0) { buf[0] = '0'; buf[1] = '\0'; }
        } else {
            format_shortest_sci(value, buf);
        }
    } else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
        format_shortest_sci(value, buf);
    } else {
        std::snprintf(buf, 32, "%.6f", value);
        size_t i = std::strlen(buf) - 1;
        while (i > 1 && buf[i] == '0' && buf[i-1] != '.') i--;
        if (i + 1 < std::strlen(buf)) buf[i + 1] = '\0';
    }
}

Value quote_string(Value v) {
    const char* content = as_cstring(v);
    if (!content) content = "";
    int quote_count = 0;
    for (const char* p = content; *p; p++) if (*p == '"') quote_count++;
    int orig_len = (int)std::strlen(content);
    int new_len = orig_len + 2 + quote_count;
    char* escaped = (char*)std::malloc((size_t)(new_len + 1));
    escaped[0] = '"';
    char* out = escaped + 1;
    for (const char* p = content; *p; p++) {
        if (*p == '"') { *out++ = '"'; *out++ = '"'; }
        else *out++ = *p;
    }
    *out++ = '"'; *out = '\0';
    Value result = make_string(escaped);
    std::free(escaped);
    return result;
}

} // namespace

Value code_form(Value v, void* vm, int recursion_limit) {
    char buf[32];

    if (is_null(v))   return make_string("null");
    if (is_double(v)) { format_double(as_double(v), buf); return make_string(buf); }
    if (is_string(v)) return quote_string(v);

    if (is_list(v)) {
        if (recursion_limit == 0) return make_string("[...]");
        if (recursion_limit > 0 && recursion_limit < 3 && vm != nullptr) {
            Value sn = find_short_name(vm, v);
            if (!is_null(sn)) return sn;
        }
        GCList list = GCManager::Lists.Get(value_item_index(v));
        Value result = make_string("[");
        for (int i = 0; i < (int)list.Items.Count(); i++) {
            if (i > 0) result = string_concat(result, make_string(", "));
            Value item = list.Items[i];
            Value item_str = is_null(item) ? make_string("null")
                                           : code_form(item, vm, recursion_limit - 1);
            result = string_concat(result, item_str);
        }
        return string_concat(result, make_string("]"));
    }

    if (is_map(v)) {
        if (recursion_limit == 0) return make_string("{...}");
        if (recursion_limit > 0 && recursion_limit < 3 && vm != nullptr) {
            Value sn = find_short_name(vm, v);
            if (!is_null(sn)) return sn;
        }
        GCMap m = GCManager::Maps.Get(value_item_index(v));
        if (m.Count() == 0) return make_string("{}");
        Value result = make_string("{");
        bool first = true;
        for (int i = m.NextEntry(-1); i != -1; i = m.NextEntry(i)) {
            if (!first) result = string_concat(result, make_string(", "));
            first = false;
            int next_limit = recursion_limit - 1;
            Value key = m.KeyAt(i);
            Value val = m.ValueAt(i);
            if (value_equal(key, val_isa_key)) next_limit = 1;
            Value key_str = code_form(key, vm, next_limit);
            Value val_str = is_null(val) ? make_string("null") : code_form(val, vm, next_limit);
            result = string_concat(result, key_str);
            result = string_concat(result, make_string(": "));
            result = string_concat(result, val_str);
        }
        return string_concat(result, make_string("}"));
    }

    if (is_funcref(v)) {
        MiniScript::FuncDef fn = funcref_funcdef(v);
        Value outer = funcref_outer_vars(v);
        if (!is_null(outer))
            std::snprintf(buf, sizeof buf, "FuncRef(%s, closure)", fn.Name().c_str());
        else
            std::snprintf(buf, sizeof buf, "FuncRef(%s)", fn.Name().c_str());
        return make_string(buf);
    }

    if (is_error(v)) {
        Value msg = error_message(v);
        if (is_string(msg)) return string_concat(make_string("error: "), msg);
        return make_string("error");
    }

    return make_string("<value>");
}

Value to_string(Value v, void* vm) {
    if (is_string(v)) return v;
    return code_form(v, vm, 3);
}

Value value_repr(Value v, void* vm) {
    return code_form(v, vm, -1);
}

Value to_number(Value v) {
    if (is_number(v)) return v;
    if (!is_string(v)) return val_zero;
    int len;
    const char* str = get_string_data_zerocopy(&v, &len);
    if (!str || len == 0) return val_zero;
    char* endptr;
    double result = std::strtod(str, &endptr);
    if (endptr == str) return val_zero;
    while (endptr < str + len && (*endptr == ' ' || *endptr == '\t'
        || *endptr == '\n' || *endptr == '\r')) endptr++;
    if (endptr != str + len) return val_zero;
    return make_double(result);
}

bool is_truthy(Value v) {
    // Falsy values: null, 0, "", [], {}.  Everything else (including
    // funcrefs and host handles) is truthy.
    if (is_null(v)) return false;
    if (is_double(v)) return as_double(v) != 0.0;
    if (is_string(v)) return string_length(v) != 0;
    if (is_list(v)) return list_count(v) != 0;
    if (is_map(v)) return map_count(v) != 0;
    return true;
}

uint32_t value_hash(Value v) {
    if (is_heap_string(v)) return get_string_hash(v);
    if (is_list(v))        return list_hash(v);
    if (is_map(v))         return map_hash(v);
    return uint64_hash((uint64_t)v);
}

// ── Frozen ──────────────────────────────────────────────────────────────

bool is_frozen(Value v) {
    if (is_list(v)) return GCManager::Lists.Get(value_item_index(v)).Frozen;
    if (is_map(v))  return GCManager::Maps.Get(value_item_index(v)).Frozen;
    return false;
}

void freeze_value(Value v) {
    if (is_list(v)) {
        int32_t idx = value_item_index(v);
        GCList l = GCManager::Lists.Get(idx);
        if (l.Frozen) return;
        GCManager::Lists.SetFrozen(idx, true);
        for (int i = 0; i < l.Items.Count(); i++) freeze_value(l.Items[i]);
    } else if (is_map(v)) {
        int32_t idx = value_item_index(v);
        GCMap m = GCManager::Maps.Get(idx);
        if (m.Frozen) return;
        GCManager::Maps.SetFrozen(idx, true);
        for (int i = m.NextEntry(-1); i != -1; i = m.NextEntry(i)) {
            freeze_value(m.KeyAt(i));
            freeze_value(m.ValueAt(i));
        }
    }
}

Value frozen_copy(Value v) {
    if (is_list(v)) {
        GCList src = GCManager::Lists.Get(value_item_index(v));
        if (src.Frozen) return v;
        Value newList = make_list((int)src.Items.Count());
        int32_t dstIdx = value_item_index(newList);
        GCList dst = GCManager::Lists.Get(dstIdx);
        GCManager::Lists.SetFrozen(dstIdx, true);
        for (int i = 0; i < src.Items.Count(); i++)
            dst.Push(frozen_copy(src.Items[i]));
        return newList;
    }
    if (is_map(v)) {
        GCMap src = GCManager::Maps.Get(value_item_index(v));
        if (src.Frozen) return v;
        Value newMap = make_map(src.Count());
        int32_t dstIdx = value_item_index(newMap);
        GCMap dst = GCManager::Maps.Get(dstIdx);
        GCManager::Maps.SetFrozen(dstIdx, true);
        for (int i = src.NextEntry(-1); i != -1; i = src.NextEntry(i))
            dst.Set(frozen_copy(src.KeyAt(i)), frozen_copy(src.ValueAt(i)));
        return newMap;
    }
    return v;
}

void set_short_name_lookup(ShortNameLookupFn fn) {
    g_short_name_lookup = fn;
}

void set_runtime_error_maker(RuntimeErrorMakerFn fn) {
    g_runtime_error_maker = fn;
}

Value value_make_runtime_error(const char* message) {
    if (g_runtime_error_maker) return g_runtime_error_maker(message);
    return make_error(make_string(message), val_null, val_null, val_null);
}

void set_stack_trace_hook(StackTraceFn fn) {
    g_stack_trace_hook = fn;
}

Value value_current_stack_trace() {
    if (g_stack_trace_hook) return g_stack_trace_hook();
    return val_null;
}

} // extern "C"
