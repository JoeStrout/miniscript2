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
#include <vector>

using MiniScript::GCManager;
using MiniScript::GCList;
using MiniScript::GCMap;
using MiniScript::GCError;
using MiniScript::GCFunction;


// ── Constants ───────────────────────────────────────────────────────────
// Static members of Value (declared in value.h).  The immediate ones are
// constant-initialized; the string ones are set in value_init_constants().
Value Value::null           = Value::fromBits(NULL_VALUE);
Value Value::Null           = Value::fromBits(NULL_VALUE);
Value Value::zero           = Value::fromBits(0x0000000000000000ULL);
Value Value::one            = Value::fromBits(0x3FF0000000000000ULL);
Value Value::emptyString    = Value::fromBits(TINY_STRING_TAG);
Value Value::magicIsA       = Value::fromBits(NULL_VALUE);
Value Value::keyString      = Value::fromBits(NULL_VALUE);
Value Value::valueString    = Value::fromBits(NULL_VALUE);
Value Value::implicitResult = Value::fromBits(NULL_VALUE);
Value Value::selfString     = Value::fromBits(NULL_VALUE);
Value Value::superString    = Value::fromBits(NULL_VALUE);

int Value::MAX_COLLECTION_SIZE = 0x7FFFFFFF;
int Value::MAP_ITER_DONE = (-2147483647 - 1);  // INT_MIN; mirrors cs/Value.cs

void value_init_constants(void) {
    Value::magicIsA       = Value::make_string("__isa");
    Value::keyString      = Value::make_string("key");
    Value::valueString    = Value::make_string("value");
    Value::implicitResult = Value::make_string("_");
    Value::selfString     = Value::make_string("self");
    Value::superString    = Value::make_string("super");
}

// ── Error accessors ─────────────────────────────────────────────────────

Value Value::make_error(Value message, Value inner, Value stack, Value isa) {
    if (stack.IsNull()) {
        stack = Value::make_list(0);
        stack.Freeze();
    }
    return GCManager::NewError(message, inner, stack, isa);
}

Value Value::Message() const {
    Value v = *this;
    if (!v.IsError()) return Value::null;
    return GCManager::Errors.Get(v.ItemIndex()).Message;
}

Value Value::Inner() const {
    Value v = *this;
    if (!v.IsError()) return Value::null;
    return GCManager::Errors.Get(v.ItemIndex()).Inner;
}

Value Value::Stack() const {
    Value v = *this;
    if (!v.IsError()) return Value::null;
    return GCManager::Errors.Get(v.ItemIndex()).Stack;
}

Value Value::Isa() const {
    Value v = *this;
    if (!v.IsError()) return Value::null;
    return GCManager::Errors.Get(v.ItemIndex()).Isa;
}

bool Value::IsaContains(Value base) const {
    Value current = *this;
    for (int depth = 0; depth < 256; depth++) {
        if (current.IsNull()) return false;
        if (current.RefEquals(base)) return true;
        if (!current.IsError()) return false;
        current = GCManager::Errors.Get(current.ItemIndex()).Isa;
    }
    return false;
}

// ── FuncRef accessors ───────────────────────────────────────────────────


Value Value::make_funcref(MiniScript::FuncDef func, Value outerVars) {
    return GCManager::NewFuncRef(func, outerVars);
}

MiniScript::FuncDef Value::FunctionDef() const {
    Value v = *this;
    if (!v.IsFuncRef()) return MiniScript::FuncDef();
    return GCManager::Functions.Get(v.ItemIndex()).Func;
}


Value Value::OuterVars() const {
    Value v = *this;
    if (!v.IsFuncRef()) return Value::null;
    return GCManager::Functions.Get(v.ItemIndex()).OuterVars;
}

// ── Arithmetic helpers ──────────────────────────────────────────────────

Value value_mult_nonnumeric(Value a, Value b) {
    if (a.IsString() && b.IsNumber()) {
        double factor = b.AsDouble();
        int factorClass = std::fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return Value::null;
        if (factor <= 0) return Value::emptyString;
        if (a.Length() * factor > Value::MAX_COLLECTION_SIZE) {
            return value_make_runtime_error("string too large (exceeds maximum size)");
        }
        int repeats = (int)factor;
        Value result = Value::emptyString;
        for (int i = 0; i < repeats; i++) result = string_concat(result, a);
        int extraChars = (int)(a.Length() * (factor - repeats));
        if (extraChars > 0) result = string_concat(result, a.Substring(0, extraChars));
        return result;
    }
    if (a.IsList() && b.IsNumber()) {
        double factor = b.AsDouble();
        int factorClass = std::fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return Value::null;
        int len = a.ListCount();
        if (factor <= 0 || len == 0) return Value::make_list(0);
        if (len * factor > Value::MAX_COLLECTION_SIZE) {
            return value_make_runtime_error("list too large (exceeds maximum size)");
        }
        int fullCopies = (int)factor;
        int extraItems = (int)(len * (factor - fullCopies));
        // Fast path: a single immutable element repeated a whole number of
        // times becomes a computed list (null increment => repeat the base).
        if (len == 1 && extraItems == 0) {
            Value elem = a.ListGet(0);
            if (elem.IsNumber() || elem.IsString() || elem.IsNull() || elem.IsFrozen()) {
                return GCManager::NewComputedList(elem, Value::null, fullCopies);
            }
        }
        Value result = Value::make_list(fullCopies * len + extraItems);
        for (int c = 0; c < fullCopies; c++)
            for (int i = 0; i < len; i++) result.Push(a.ListGet(i));
        for (int i = 0; i < extraItems; i++) result.Push(a.ListGet(i));
        return result;
    }
    return Value::null;
}

// Scalar (non-collection) equality: numbers by value, strings by content,
// null with null, and everything else (funcref/error/handle) by reference
// identity.  Lists and maps are NOT handled here -- RecursiveEqual walks them
// iteratively so that reference cycles cannot recurse forever.
static bool value_equal_scalar(Value a, Value b) {
    if (a.RefEquals(b)) return true;
    if (a.IsNumber() && b.IsNumber()) return a.AsDouble() == b.AsDouble();
    if (a.IsString() && b.IsString()) return string_equals(a, b);
    if (a.IsNull()   && b.IsNull())   return true;
    // Same-type non-container reference values compare by identity; since
    // RefEquals already failed above, two distinct such values are not equal.
    return false;
}

// A pair of Values to compare, used by RecursiveEqual's work-list below.
namespace {
    struct ValuePair {
        Value a;
        Value b;
        // Careful: compare with RefEquals, not == (deep), so that the visited
        // set can detect reference loops without recursing back into RecursiveEqual.
        bool ref_equal(const ValuePair& other) const {
            return a.RefEquals(other.a) && b.RefEquals(other.b);
        }
    };
    bool visited_contains(const std::vector<ValuePair>& visited, const ValuePair& p) {
        for (size_t i = 0; i < visited.size(); i++)
            if (visited[i].ref_equal(p)) return true;
        return false;
    }
}

// Deep MiniScript equality (the implementation behind operator==).  Scalars are
// compared directly; lists and maps are compared with an explicit work-list
// (toDo) and a visited set, mirroring MS1's Value::RecursiveEqual.  The visited
// set (compared by reference, not deep) means cyclic structures terminate
// instead of overflowing the stack.
bool Value::RecursiveEqual(Value rhs) const {
    Value a = *this, b = rhs;
    if (a.RefEquals(b)) return true;
    // Hot path: if neither side is a collection, no work-list is needed.
    if (!(a.IsList() || a.IsMap()) && !(b.IsList() || b.IsMap()))
        return value_equal_scalar(a, b);

    std::vector<ValuePair> toDo;
    std::vector<ValuePair> visited;
    ValuePair start; start.a = a; start.b = b;
    toDo.push_back(start);
    while (!toDo.empty()) {
        ValuePair pair = toDo.back();
        toDo.pop_back();
        visited.push_back(pair);
        Value pa = pair.a, pb = pair.b;
        if (pa.IsList()) {
            if (!pb.IsList()) return false;
            int aCount = pa.ListCount();
            if (pb.ListCount() != aCount) return false;
            if (pa.RefEquals(pb)) continue;  // same list object: nothing to do
            for (int i = 0; i < aCount; i++) {
                ValuePair np; np.a = pa.ListGet(i); np.b = pb.ListGet(i);
                if (!visited_contains(visited, np)) toDo.push_back(np);
            }
        } else if (pa.IsMap()) {
            if (!pb.IsMap()) return false;
            GCMap mA = GCManager::Maps.Get(pa.ItemIndex());
            GCMap mB = GCManager::Maps.Get(pb.ItemIndex());
            if (mA.Count() != mB.Count()) return false;
            if (pa.RefEquals(pb)) continue;  // same map object: nothing to do
            for (int i = mA.NextEntry(-1); i != -1; i = mA.NextEntry(i)) {
                Value bv;
                if (!mB.TryGet(mA.KeyAt(i), &bv)) return false;
                ValuePair np; np.a = mA.ValueAt(i); np.b = bv;
                if (!visited_contains(visited, np)) toDo.push_back(np);
            }
        } else {
            // No other types can recurse, so compare them directly.
            if (!value_equal_scalar(pa, pb)) return false;
        }
    }
    // Drained the work-list without finding any inequality: the values are equal.
    return true;
}

int value_compare(Value a, Value b) {
    if (a.IsNumber() && b.IsNumber()) {
        double da = a.NumericVal(), db = b.NumericVal();
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
    if (a.IsString() && b.IsString()) return string_compare(a, b);
    int ta = a.IsNumber() ? 0 : a.IsString() ? 1 : 2;
    int tb = b.IsNumber() ? 0 : b.IsString() ? 1 : 2;
    return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}

// ── Bitwise ops (cast to int64, operate, cast back to double) ────────────

Value value_xor(Value a, Value b) {
    return Value((double)((int64_t)a.AsDouble() ^ (int64_t)b.AsDouble()));
}
Value value_unary(Value a) {
    return Value((double)(~(int64_t)a.AsDouble()));
}
Value value_shr(Value v, int shift) {
    return Value((double)((int64_t)v.AsDouble() >> shift));
}
Value value_shl(Value v, int shift) {
    return Value((double)((int64_t)v.AsDouble() << shift));
}

// ── code_form / to_string / to_number ───────────────────────────────────

namespace {

ShortNameLookupFn g_short_name_lookup = nullptr;
RuntimeErrorMakerFn g_runtime_error_maker = nullptr;
StackTraceFn g_stack_trace_hook = nullptr;

Value find_short_name(void* vm, Value v) {
    if (!vm || !g_short_name_lookup) return Value::null;
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
    const char* content = v.AsCString();
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
    Value result = Value::make_string(escaped);
    std::free(escaped);
    return result;
}

} // namespace

Value code_form(Value v, void* vm, int recursion_limit) {
    char buf[32];

    if (v.IsNull())   return Value::make_string("null");
    if (v.IsNumber()) { format_double(v.AsDouble(), buf); return Value::make_string(buf); }
    if (v.IsString()) return quote_string(v);

    if (v.IsList()) {
        if (recursion_limit == 0) return Value::make_string("[...]");
        if (recursion_limit > 0 && recursion_limit < 3 && vm != nullptr) {
            Value sn = find_short_name(vm, v);
            if (!sn.IsNull()) return sn;
        }
        GCList list = GCManager::Lists.Get(v.ItemIndex());
        int listCount = list.Count();
        Value result = Value::make_string("[");
        for (int i = 0; i < listCount; i++) {
            if (i > 0) result = string_concat(result, Value::make_string(", "));
            Value item = list.Get(i);
            Value item_str = item.IsNull() ? Value::make_string("null")
                                           : code_form(item, vm, recursion_limit - 1);
            result = string_concat(result, item_str);
        }
        return string_concat(result, Value::make_string("]"));
    }

    if (v.IsMap()) {
        if (recursion_limit == 0) return Value::make_string("{...}");
        if (recursion_limit > 0 && recursion_limit < 3 && vm != nullptr) {
            Value sn = find_short_name(vm, v);
            if (!sn.IsNull()) return sn;
        }
        GCMap m = GCManager::Maps.Get(v.ItemIndex());
        if (m.Count() == 0) return Value::make_string("{}");
        Value result = Value::make_string("{");
        bool first = true;
        for (int i = m.NextEntry(-1); i != -1; i = m.NextEntry(i)) {
            if (!first) result = string_concat(result, Value::make_string(", "));
            first = false;
            int next_limit = recursion_limit - 1;
            Value key = m.KeyAt(i);
            Value val = m.ValueAt(i);
            if (key.RecursiveEqual(Value::magicIsA)) next_limit = 1;
            Value key_str = code_form(key, vm, next_limit);
            Value val_str = val.IsNull() ? Value::make_string("null") : code_form(val, vm, next_limit);
            result = string_concat(result, key_str);
            result = string_concat(result, Value::make_string(": "));
            result = string_concat(result, val_str);
        }
        return string_concat(result, Value::make_string("}"));
    }

    if (v.IsFuncRef()) {
        MiniScript::FuncDef fn = v.FunctionDef();
        Value outer = v.OuterVars();
        if (!outer.IsNull())
            std::snprintf(buf, sizeof buf, "FuncRef(%s, closure)", fn.Name().c_str());
        else
            std::snprintf(buf, sizeof buf, "FuncRef(%s)", fn.Name().c_str());
        return Value::make_string(buf);
    }

    if (v.IsError()) {
        Value msg = v.Message();
        if (msg.IsString()) return string_concat(Value::make_string("error: "), msg);
        return Value::make_string("error");
    }

    return Value::make_string("<value>");
}

Value Value::ToStringValue(void* vm) const {
    Value v = *this;
    if (v.IsString()) return v;
    return code_form(v, vm, 3);
}

Value Value::Repr(void* vm) const {
    Value v = *this;
    return code_form(v, vm, -1);
}

Value Value::ToNumber() const {
    Value v = *this;
    if (v.IsNumber()) return v;
    if (!v.IsString()) return Value::zero;
    int len;
    const char* str = get_string_data_zerocopy(&v, &len);
    if (!str || len == 0) return Value::zero;
    char* endptr;
    double result = std::strtod(str, &endptr);
    if (endptr == str) return Value::zero;
    while (endptr < str + len && (*endptr == ' ' || *endptr == '\t'
        || *endptr == '\n' || *endptr == '\r')) endptr++;
    if (endptr != str + len) return Value::zero;
    return Value(result);
}


uint32_t value_hash(Value v) {
    if (v.IsHeapString()) return get_string_hash(v);
    if (v.IsList())        return list_hash(v);
    if (v.IsMap())         return map_hash(v);
    return uint64_hash(v.bits);
}

// ── Frozen ──────────────────────────────────────────────────────────────

bool Value::IsFrozen() const {
    Value v = *this;
    if (v.IsList()) return GCManager::Lists.Get(v.ItemIndex()).Frozen;
    if (v.IsMap())  return GCManager::Maps.Get(v.ItemIndex()).Frozen;
    return false;
}

void Value::Freeze() const {
    Value v = *this;
    if (v.IsList()) {
        int32_t idx = v.ItemIndex();
        GCList l = GCManager::Lists.Get(idx);
        if (l.Frozen) return;
        GCManager::Lists.SetFrozen(idx, true);
        if (l.Computed) {
            // Computed-list elements derive from an immutable base; freezing
            // the base covers them all without materializing the list.
            l.Get(0).Freeze();
        } else {
            int n = l.Count();
            for (int i = 0; i < n; i++) l.Get(i).Freeze();
        }
    } else if (v.IsMap()) {
        int32_t idx = v.ItemIndex();
        GCMap m = GCManager::Maps.Get(idx);
        if (m.Frozen) return;
        GCManager::Maps.SetFrozen(idx, true);
        for (int i = m.NextEntry(-1); i != -1; i = m.NextEntry(i)) {
            m.KeyAt(i).Freeze();
            m.ValueAt(i).Freeze();
        }
    }
}

Value Value::FrozenCopy() const {
    Value v = *this;
    if (v.IsList()) {
        GCList src = GCManager::Lists.Get(v.ItemIndex());
        if (src.Frozen) return v;
        int srcCount = src.Count();
        Value newList = Value::make_list(srcCount);
        int32_t dstIdx = newList.ItemIndex();
        GCList dst = GCManager::Lists.Get(dstIdx);
        GCManager::Lists.SetFrozen(dstIdx, true);
        for (int i = 0; i < srcCount; i++)
            dst.Push(src.Get(i).FrozenCopy());
        return newList;
    }
    if (v.IsMap()) {
        GCMap src = GCManager::Maps.Get(v.ItemIndex());
        if (src.Frozen) return v;
        Value newMap = Value::make_map(src.Count());
        int32_t dstIdx = newMap.ItemIndex();
        GCMap dst = GCManager::Maps.Get(dstIdx);
        GCManager::Maps.SetFrozen(dstIdx, true);
        for (int i = src.NextEntry(-1); i != -1; i = src.NextEntry(i))
            dst.Set(src.KeyAt(i).FrozenCopy(), src.ValueAt(i).FrozenCopy());
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
    return Value::make_error(Value::make_string(message), Value::null, Value::null, Value::null);
}

void set_stack_trace_hook(StackTraceFn fn) {
    g_stack_trace_hook = fn;
}

Value Value::value_current_stack_trace() {
    if (g_stack_trace_hook) return g_stack_trace_hook();
    return Value::null;
}

