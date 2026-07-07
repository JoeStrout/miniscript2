// cstr_arena.h — per-native-call scratch storage for C-string views of Values.
//
// ============================================================================
// MANDATE (why this file exists — read before touching it)
// ----------------------------------------------------------------------------
// Host code constantly needs a `const char*` from a MiniScript string to pass
// to a C API (file names, text, gamepad mappings, …).  The obvious idiom
//
//     const char* p = value.ToString().c_str();   // <-- USE AFTER FREE
//
// is a bug: `String` owns its bytes through a shared_ptr<StringStorage>, so the
// temporary returned by ToString() frees that buffer at the end of the
// full-expression, leaving `p` dangling.  (It "works" for interned literals
// that the intern table co-owns, which is exactly what makes the bug
// intermittent and hard to spot.)
//
// `Value::c_str()` avoids this by copying the bytes into THIS arena and handing
// back a pointer with one simple, predictable lifetime:
//
//     **valid until the current native (intrinsic) call returns.**
//
// The VM brackets every native callback with GetMark()/Reset()
// (see VM.InvokeNativeCallback in cs/VM.cs), so each intrinsic invocation gets a
// fresh region that is rewound when it returns.  That bounds memory and defines
// the lifetime above.  Nested / re-entrant callbacks (e.g. VM::RunFunction)
// nest correctly: each invocation saves its own Mark and restores it on exit,
// so an inner call's Reset never frees an outer call's still-live pointers.
//
// This file is the ONLY owner of that storage.  Value::c_str() (and any future
// Context::GetArgCStr helper) must be thin wrappers over Copy(); nothing else
// should reach into the arena.
//
// Contract details:
//   * Copy() never returns null; an empty/na value yields "".
//   * Returned pointers stay valid until the enclosing call's Reset.  Chunks are
//     never reallocated once handed out, so allocating more within the same call
//     does NOT invalidate earlier pointers (you can hold several at once, e.g.
//     TextFindIndex(a.c_str(), b.c_str())).
//   * Used OUTSIDE any intrinsic call there is no enclosing bracket, so the
//     allocation is NOT auto-reclaimed: it stays valid (never dangles) until you
//     call ResetAll() or the thread exits.  Intervening intrinsic calls do NOT
//     free it — their Reset only rewinds to a mark captured ABOVE your
//     allocation.  Fine for occasional host use (a filename, a global); for
//     heavy or looping host-side use, wrap the block in a CStrArena::Scope (or
//     call ResetAll() at a safe point) so it doesn't accumulate.  We deliberately
//     do NOT reset per VM.Run() batch, so a pointer may safely span a Run() call.
//
// Header-only on purpose: no .cpp means no build-file edits in either the MS2
// or host build, and the thread_local state lives in one inline-function local.
// ============================================================================

#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace MiniScript {

class CStrArena {
public:
	// Opaque cursor into the arena, captured on entry to a native call and
	// restored on exit.  Cheap to copy (two ints); lives on the C++ stack.
	struct Mark { uint32_t chunk; uint32_t offset; };

	// Capture the current allocation position (call on entry to a native call).
	static Mark GetMark() {
		State& s = tls();
		return Mark{ s.cur, s.off };
	}

	// Rewind to a previously captured position (call on exit).  Everything
	// allocated since that Mark becomes free for reuse; pointers into it are
	// no longer valid.
	static void Reset(Mark m) {
		State& s = tls();
		s.cur = m.chunk;
		s.off = m.offset;
	}

	// Copy `len` bytes plus a NUL terminator into the arena and return a stable
	// pointer to the copy.  This is the one primitive Value::c_str() uses.
	static const char* Copy(const char* bytes, int len) {
		if (len < 0) len = 0;
		State& s = tls();
		uint32_t need = (uint32_t)len + 1;   // + NUL terminator
		Chunk& c = s.ensure(need);
		char* dst = c.data + s.off;
		if (len > 0 && bytes) memcpy(dst, bytes, (size_t)len);
		dst[len] = '\0';
		s.off += need;
		return dst;
	}

	// Host convenience: drop ALL outstanding allocations at once (e.g. once per
	// frame if you use c_str() outside of intrinsic calls).  Keeps the chunks
	// allocated for reuse.
	static void ResetAll() {
		State& s = tls();
		s.cur = 0;
		s.off = 0;
	}

	// RAII bracket for HOST code that uses Value::c_str() outside of an intrinsic
	// call: marks the arena on construction and rewinds on destruction, so any
	// pointers made within the scope are reclaimed when it ends.  This is the
	// manual equivalent of the bracket the VM puts around every intrinsic call;
	// reach for it in host-side loops (e.g. once per frame) so c_str() results
	// don't accumulate.  Pointers made in the scope must not escape it.
	//
	//     for (...) {
	//         MiniScript::CStrArena::Scope arena;   // reclaimed each iteration
	//         SomeCApi(v.c_str());
	//     }
	class Scope {
	public:
		Scope() : mark_(CStrArena::GetMark()) {}
		~Scope() { CStrArena::Reset(mark_); }
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	private:
		Mark mark_;
	};

private:
	static const uint32_t kChunkSize = 8 * 1024;

	struct Chunk {
		char* data;
		uint32_t cap;
	};

	struct State {
		std::vector<Chunk> chunks;
		uint32_t cur = 0;   // index of the current chunk
		uint32_t off = 0;   // bump offset within the current chunk

		~State() {
			for (size_t i = 0; i < chunks.size(); i++) free(chunks[i].data);
		}

		// Position at a chunk with room for `need` bytes, allocating one if
		// necessary, and return it.  Existing chunks are reused (and overwritten)
		// after a Reset; oversized requests get a dedicated chunk sized to fit.
		// Chunks already handed out are never moved, so outstanding pointers from
		// the current call stay valid.
		Chunk& ensure(uint32_t need) {
			if (cur < chunks.size() && off + need <= chunks[cur].cap) {
				return chunks[cur];
			}
			// Advance into the next already-allocated chunk that fits.
			while (cur + 1 < chunks.size()) {
				cur++;
				off = 0;
				if (need <= chunks[cur].cap) return chunks[cur];
			}
			// None fit — allocate a fresh chunk (oversized if the request is big).
			uint32_t cap = need > kChunkSize ? need : kChunkSize;
			Chunk nc;
			nc.data = (char*)malloc(cap);
			nc.cap = cap;
			chunks.push_back(nc);
			cur = (uint32_t)chunks.size() - 1;
			off = 0;
			return chunks[cur];
		}
	};

	// The single owner of arena state: one instance per thread.
	static State& tls() {
		static thread_local State s;
		return s;
	}
};

} // namespace MiniScript
