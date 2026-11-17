// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: PerProgram.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// PerProgram.cs
// A value that each program gets its own copy of -- the per-program analog of
// a thread-local variable.
// Intrinsics often hand scripts a map: the `list` and `string` type maps, a
// `file` module, a class map such as `FileHandle`.  Scripts can modify those
// maps, so if the intrinsic returns one process-wide map, one script's changes
// are seen by every other script in the process.  Instead, declare a PerProgram
// once, with a function that builds the value, and ask it for the value
// whenever an intrinsic needs it:
//     // once, at startup (e.g. in your intrinsics' Init):
//     _fileModule = new PerProgram(BuildFileModuleMap, "file");
//     // in an intrinsic:
//     return new IntrinsicResult(_fileModule.Get(ctx));
// The first time a program asks, the builder is called and the result is
// stored in that program's global namespace (Globals); later requests return
// the same value.  So:
//   * each program (each Interpreter) has its own value;
//   * a chained program (Interpreter.ResetPreservingGlobals) keeps it;
//   * a new program (Interpreter.Reset) starts fresh;
//   * `reset` discards it, and the next request builds a new one;
//   * the namespace keeps it alive, so it needs no GC root.
// The builder must not return null: null is how an unbuilt value is recorded,
// so a builder that returns it would simply be called again on every request.
// The optional display name is what the value prints as when it appears
// inside another value (as `list` does in `[list, string]`).  Pass "" for none.

#include "value.h"
#include <functional>
#include "IntrinsicAPI.g.h"
#include "Globals.g.h"

namespace MiniScript {
typedef std::function<Value()> PerProgramBuilder;
inline bool IsNull(PerProgramBuilder f) { return f == nullptr; }

// DECLARATIONS

class PerProgramStorage : public std::enable_shared_from_this<PerProgramStorage> {
	friend struct PerProgram;
	private: static List<String> _displayNames;
	private: Int32 _index; // this value's position in each Globals
	private: PerProgramBuilder _builder; // makes a fresh value

	// Display name for each index ("" for none), indexed by _index.

	public: PerProgramStorage(PerProgramBuilder builder, String displayName = "");

	// The calling program's value, building it if this program has none yet.
	public: Value Get(Context ctx);

	// As above, for code that has no Context: uses the VM that is currently
	// running.  With no running VM there is no program to store the value in,
	// so this returns a freshly built value that is not kept.
	public: Value Get();

	// The value for the program whose global namespace is g.  If g is null,
	// returns a freshly built value that is not kept.
	public: Value GetFor(Globals g);

	// The calling program's value if it has been built, or null if not.
	// Never builds one.
	public: Value Peek(Context ctx);

	// The display name registered for the PerProgram at the given index
	// ("" if none).  Used by Globals.PerProgramName.
	public: static String DisplayName(Int32 index);
}; // end of class PerProgramStorage

// Function that builds a fresh per-program value.

struct PerProgram {
	friend class PerProgramStorage;
	protected: std::shared_ptr<PerProgramStorage> storage;
  public:
	PerProgram(std::shared_ptr<PerProgramStorage> stor) : storage(stor) {}
	PerProgram() : storage(nullptr) {}
	PerProgram(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const PerProgram& inst) { return inst.storage == nullptr; }
	private: PerProgramStorage* get() const;

	private: List<String> _displayNames();
	private: void set__displayNames(List<String> _v);
	private: Int32 _index(); // this value's position in each Globals
	private: void set__index(Int32 _v); // this value's position in each Globals
	private: PerProgramBuilder _builder(); // makes a fresh value
	private: void set__builder(PerProgramBuilder _v); // makes a fresh value

	// Display name for each index ("" for none), indexed by _index.

	public: static PerProgram New(PerProgramBuilder builder, String displayName = "") {
		return PerProgram(std::make_shared<PerProgramStorage>(builder, displayName));
	}

	// The calling program's value, building it if this program has none yet.
	public: inline Value Get(Context ctx);

	// As above, for code that has no Context: uses the VM that is currently
	// running.  With no running VM there is no program to store the value in,
	// so this returns a freshly built value that is not kept.
	public: inline Value Get();

	// The value for the program whose global namespace is g.  If g is null,
	// returns a freshly built value that is not kept.
	public: inline Value GetFor(Globals g);

	// The calling program's value if it has been built, or null if not.
	// Never builds one.
	public: inline Value Peek(Context ctx);

	// The display name registered for the PerProgram at the given index
	// ("" if none).  Used by Globals.PerProgramName.
	public: static String DisplayName(Int32 index) { return PerProgramStorage::DisplayName(index); }
}; // end of struct PerProgram

// INLINE METHODS

inline PerProgramStorage* PerProgram::get() const { return static_cast<PerProgramStorage*>(storage.get()); }
inline List<String> PerProgram::_displayNames() { return get()->_displayNames; }
inline void PerProgram::set__displayNames(List<String> _v) { get()->_displayNames = _v; }
inline Int32 PerProgram::_index() { return get()->_index; } // this value's position in each Globals
inline void PerProgram::set__index(Int32 _v) { get()->_index = _v; } // this value's position in each Globals
inline PerProgramBuilder PerProgram::_builder() { return get()->_builder; } // makes a fresh value
inline void PerProgram::set__builder(PerProgramBuilder _v) { get()->_builder = _v; } // makes a fresh value
inline Value PerProgram::Get(Context ctx) { return get()->Get(ctx); }
inline Value PerProgram::Get() { return get()->Get(); }
inline Value PerProgram::GetFor(Globals g) { return get()->GetFor(g); }
inline Value PerProgram::Peek(Context ctx) { return get()->Peek(ctx); }

} // end of namespace MiniScript
