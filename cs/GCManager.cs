//*** BEGIN CS_ONLY ***
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace MiniScript {

/// <summary>
/// Central GC coordinator.  Owns the five typed GCSets and an explicit root list.
/// Mark(Value) dispatches to the right GCSet using the GCSet index baked into
/// the Value bits — no switch statement, just array indexing.
/// </summary>
public sealed class GCManager {

	// GCSet indices — these constants define the encoding baked into every GC Value.
	public const int STRING_SET  = 0;
	public const int LIST_SET    = 1;
	public const int MAP_SET     = 2;
	public const int ERROR_SET   = 3;
	public const int FUNCREF_SET = 4;

	// Typed accessors; use these to allocate new objects.
	public readonly GCSet<GCString>  Strings  = new();
	public readonly GCSet<GCList>    Lists    = new();
	public readonly GCSet<GCMap>     Maps     = new();
	public readonly GCSet<GCError>   Errors   = new();
	public readonly GCSet<GCFuncRef> FuncRefs = new();

	// Unified array for branchless dispatch in Mark().
	private readonly IGCSet[] _sets;

	private readonly List<Value> _roots = new();

	public GCManager() {
		_sets = new IGCSet[] { Strings, Lists, Maps, Errors, FuncRefs };
	}

	// ── Value factories ──────────────────────────────────────────────────────

	public Value NewString(string s) {
		int idx = Strings.New();
		Strings.Get(idx).Data = s;
		return Value.MakeGC(STRING_SET, idx);
	}

	public Value NewList(int capacity = 8) {
		int idx = Lists.New();
		Lists.Get(idx).Init(capacity);
		return Value.MakeGC(LIST_SET, idx);
	}

	public Value NewMap(int capacity = 8) {
		int idx = Maps.New();
		Maps.Get(idx).Init(capacity);
		return Value.MakeGC(MAP_SET, idx);
	}

	public Value NewVarMap(VarMapBacking vmb) {
		int idx = Maps.New();
		ref GCMap m = ref Maps.Get(idx);
		m.Init(4);
		m._vmb = vmb;
		return Value.MakeGC(MAP_SET, idx);
	}

	public Value NewError(Value message, Value inner, Value stack, Value isa) {
		int idx = Errors.New();
		ref GCError e = ref Errors.Get(idx);
		e.Message = message;
		e.Inner   = inner;
		e.Stack   = stack;
		e.Isa     = isa;
		return Value.MakeGC(ERROR_SET, idx);
	}

	public Value NewFuncRef(int funcIndex, Value outerVars) {
		int idx = FuncRefs.New();
		ref GCFuncRef f = ref FuncRefs.Get(idx);
		f.FuncIndex = funcIndex;
		f.OuterVars = outerVars;
		return Value.MakeGC(FUNCREF_SET, idx);
	}

	// ── Retain / Release ─────────────────────────────────────────────────────

	public void Retain(Value v) {
		if (!v.IsGCObject) return;
		_sets[v.GCSetIndex].Mark(v.ItemIndex, this);  // not really right; kept for API compat
	}

	public void RetainValue(Value v) {
		if (!v.IsGCObject) return;
		_sets[v.GCSetIndex].Mark(v.ItemIndex, this);  // placeholder; use explicit Retain when needed
	}

	// ── Root set ─────────────────────────────────────────────────────────────

	public void AddRoot(Value v)    => _roots.Add(v);
	public void RemoveRoot(Value v) => _roots.Remove(v);
	public void ClearRoots()        => _roots.Clear();

	// ── GC cycle ─────────────────────────────────────────────────────────────

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public void Mark(Value v) {
		if (!v.IsGCObject) return;
		_sets[v.GCSetIndex].Mark(v.ItemIndex, this);
	}

	/// <summary>Run a full mark-sweep cycle.</summary>
	public void CollectGarbage() {
		// 1. Clear all mark bits.
		foreach (var set in _sets) set.PrepareForGC();

		// 2. Mark from explicit roots.
		foreach (var root in _roots) Mark(root);

		// 3. Mark retained items (and their children).
		foreach (var set in _sets) set.MarkRetained(this);

		// 4. Sweep: free everything still unmarked.
		foreach (var set in _sets) set.Sweep();
	}

	// ── Diagnostics ──────────────────────────────────────────────────────────

	public void PrintStats() {
		Console.WriteLine($"  Strings:  {Strings.LiveCount}");
		Console.WriteLine($"  Lists:    {Lists.LiveCount}");
		Console.WriteLine($"  Maps:     {Maps.LiveCount}");
		Console.WriteLine($"  Errors:   {Errors.LiveCount}");
		Console.WriteLine($"  FuncRefs: {FuncRefs.LiveCount}");
	}

	// ── Convenience accessors ─────────────────────────────────────────────────

	public ref GCString  GetString(Value v)  => ref Strings.Get(v.ItemIndex);
	public ref GCList    GetList(Value v)     => ref Lists.Get(v.ItemIndex);
	public ref GCMap     GetMap(Value v)      => ref Maps.Get(v.ItemIndex);
	public ref GCError   GetError(Value v)    => ref Errors.Get(v.ItemIndex);
	public ref GCFuncRef GetFuncRef(Value v)  => ref FuncRefs.Get(v.ItemIndex);

	// ── Static helper for content-based string access ─────────────────────────
	// Used by GCMap for content-based key hashing and equality.

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static string GetStringContent(Value v) {
		if (v.IsTiny) {
			int len = v.TinyLen();
			var chars = new char[len];
			for (int i = 0; i < len; i++) chars[i] = (char)((v.Bits >> (8 * (i + 1))) & 0xFF);
			return new string(chars);
		}
		if (v.IsGCObject && v.GCSetIndex == STRING_SET) {
			// Static accessor — requires GC to be accessible.
			return ValueHelpers.gc.Strings.Get(v.ItemIndex).Data ?? "";
		}
		return "";
	}
}

}
//*** END CS_ONLY ***
