// PerProgram.cs
//
// A value that each program gets its own copy of -- the per-program analog of
// a thread-local variable.
//
// Intrinsics often hand scripts a map: the `list` and `string` type maps, a
// `file` module, a class map such as `FileHandle`.  Scripts can modify those
// maps, so if the intrinsic returns one process-wide map, one script's changes
// are seen by every other script in the process.  Instead, declare a PerProgram
// once, with a function that builds the value, and ask it for the value
// whenever an intrinsic needs it:
//
//     // once, at startup (e.g. in your intrinsics' Init):
//     _fileModule = new PerProgram(BuildFileModuleMap, "file");
//
//     // in an intrinsic:
//     return new IntrinsicResult(_fileModule.Get(ctx));
//
// The first time a program asks, the builder is called and the result is
// stored in that program's global namespace (Globals); later requests return
// the same value.  So:
//   * each program (each Interpreter) has its own value;
//   * a chained program (Interpreter.ResetPreservingGlobals) keeps it;
//   * a new program (Interpreter.Reset) starts fresh;
//   * `reset` discards it, and the next request builds a new one;
//   * the namespace keeps it alive, so it needs no GC root.
//
// The builder must not return null: null is how an unbuilt value is recorded,
// so a builder that returns it would simply be called again on every request.
//
// The optional display name is what the value prints as when it appears
// inside another value (as `list` does in `[list, string]`).  Pass "" for none.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "value.h"
// H: #include <functional>
// H: #include "IntrinsicAPI.g.h"
// H: #include "Globals.g.h"
// CPP: #include "VM.g.h"

namespace MiniScript {

// Function that builds a fresh per-program value.
// H: typedef std::function<Value()> PerProgramBuilder;
// H: inline bool IsNull(PerProgramBuilder f) { return f == nullptr; }
public delegate Value PerProgramBuilder(); // CPP:

public class PerProgram {

	// Display name for each index ("" for none), indexed by _index.
	private static List<String> _displayNames = new List<String>();

	private Int32 _index;                 // this value's position in each Globals
	private PerProgramBuilder _builder;   // makes a fresh value

	public PerProgram(PerProgramBuilder builder, String displayName = "") {
		_builder = builder;
		_index = _displayNames.Count;
		_displayNames.Add(displayName);
	}

	// The calling program's value, building it if this program has none yet.
	public Value Get(Context ctx) {
		return GetFor(ctx.vm.GetGlobals());
	}

	// As above, for code that has no Context: uses the VM that is currently
	// running.  With no running VM there is no program to store the value in,
	// so this returns a freshly built value that is not kept.
	public Value Get() {
		VM vm = VM.ActiveVM();
		if (vm == null) return _builder();
		return GetFor(vm.GetGlobals());
	}

	// The value for the program whose global namespace is g.  If g is null,
	// returns a freshly built value that is not kept.
	public Value GetFor(Globals g) {
		if (g == null) return _builder();
		Value v = g.PerProgramValue(_index);
		if (v.IsNull()) {
			v = _builder();
			g.SetPerProgramValue(_index, v);
		}
		return v;
	}

	// The calling program's value if it has been built, or null if not.
	// Never builds one.
	public Value Peek(Context ctx) {
		Globals g = ctx.vm.GetGlobals();
		if (g == null) return Value.Null;
		return g.PerProgramValue(_index);
	}

	// The display name registered for the PerProgram at the given index
	// ("" if none).  Used by Globals.PerProgramName.
	public static String DisplayName(Int32 index) {
		if (index < 0 || index >= _displayNames.Count) return "";
		return _displayNames[index];
	}
}

}
