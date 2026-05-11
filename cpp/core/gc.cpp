// gc.cpp — shim implementations forwarding to the new GCManager.
//
// Most legacy entry points are no-ops or thin forwarders. Only
// gc_register_mark_callback / gc_mark_value carry real behaviour, since
// they're how the VM injects roots into the collector.

#include "gc.h"
#include "GCManager.h"
#include <cstdlib>
#include <vector>
#include <utility>

using MiniScript::GCManager;
using MiniScript::MarkCallback;

namespace {

// Adapter: the legacy callback signature is `(void* user_data)`, the new
// signature is `(void* user_data, GCManager& gc)`. We register a per-callback
// adapter pointer that closes over the legacy callback.
struct LegacyCallback {
    gc_mark_callback_t fn;
    void*              user_data;
};

// Stable storage for adapter records so their addresses remain valid
// while registered with GCManager.
std::vector<LegacyCallback*>& legacyCallbacks() {
    static std::vector<LegacyCallback*> v;
    return v;
}

void legacy_adapter(void* user_data, MiniScript::GCManager& /*gc*/) {
    LegacyCallback* lc = static_cast<LegacyCallback*>(user_data);
    if (lc && lc->fn) lc->fn(lc->user_data);
}

bool _gcEnabled = true;
int  _collectionsCount = 0;

} // namespace

extern "C" {

// ── Lifecycle ───────────────────────────────────────────────────────────

void gc_init(void)     { /* GCManager is constructed on first Instance() call */ }
void gc_shutdown(void) { /* leaks are acceptable at process exit */ }

void* gc_allocate(size_t size) {
    // Legacy callers expect zero-initialised memory.
    void* p = std::malloc(size > 0 ? size : 1);
    if (p) std::memset(p, 0, size);
    return p;
}

// ── Collection control ──────────────────────────────────────────────────

void gc_collect(void) {
    if (!_gcEnabled) return;
    GCManager::Instance().CollectGarbage();
    _collectionsCount++;
}

void gc_disable(void) { _gcEnabled = false; }
void gc_enable(void)  { _gcEnabled = true;  }

// ── Local-variable protection (no-ops) ──────────────────────────────────

void gc_protect_value(Value* /*val_ptr*/) {}
void gc_unprotect_value(void)             {}

// ── Mark callbacks ──────────────────────────────────────────────────────

void gc_register_mark_callback(gc_mark_callback_t callback, void* user_data) {
    LegacyCallback* lc = new LegacyCallback{callback, user_data};
    legacyCallbacks().push_back(lc);
    GCManager::Instance().RegisterMarkCallback(legacy_adapter, lc);
}

void gc_unregister_mark_callback(gc_mark_callback_t callback, void* user_data) {
    auto& cbs = legacyCallbacks();
    for (auto it = cbs.begin(); it != cbs.end(); ++it) {
        if ((*it)->fn == callback && (*it)->user_data == user_data) {
            GCManager::Instance().UnregisterMarkCallback(legacy_adapter, *it);
            delete *it;
            cbs.erase(it);
            return;
        }
    }
}

void gc_mark_value(Value v) {
    GCManager::Instance().Mark(v);
}

// ── Stats ───────────────────────────────────────────────────────────────

GCStats gc_get_stats(void) {
    GCStats s;
    s.bytes_allocated  = 0;
    s.gc_threshold     = 0;
    s.collections_count = _collectionsCount;
    s.root_count       = 0;
    s.scope_depth      = 0;
    s.max_scope_depth  = 0;
    s.is_enabled       = _gcEnabled;
    return s;
}

int gc_get_scope_depth(void) { return 0; }

} // extern "C"
