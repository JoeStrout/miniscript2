// Intrinsic.cs - The Intrinsic class: a built-in function registered as a callable FuncRef.
// Each intrinsic is defined with a builder-style API:
//   f = Intrinsic.Create("name");
//   f.AddParam("paramName", defaultValue);
//   f.AddParam("other", defaultValue, true);   // this one accepts an error argument
//   f.AffectsState = true;                      // if it changes state or returns nothing
//   f.Code = (Context ctx, IntrinsicResult partialResult) => { ... };
// An error argument to any parameter not declared to accept one never reaches
// Code: the call evaluates to that error, or terminates if AffectsState.

using System;
using System.Collections.Generic;
// H: #include "value.h"
// H: #include "FuncDef.g.h"
// CPP: #include "CoreIntrinsics.g.h"

namespace MiniScript {

public class Intrinsic {
	public String Name;

	public NativeCallbackDelegate Code;

	private List<String> _paramNames;
	private List<Value> _paramDefaults;
	private UInt32 _acceptsErrorMask = 0;

	// Set for an intrinsic that changes state (or returns nothing), so that an
	// error argument it did not ask for terminates rather than being returned.
	public Boolean AffectsState = false;
	private FuncDef _funcDef = null;
	private Value _funcRef = Value.Null;

	private static List<Intrinsic> _all = new List<Intrinsic>();
	private static Dictionary<String, Intrinsic> _byName = new Dictionary<String, Intrinsic>();
	private static Boolean _initialized = false;
	private static Boolean _markCallbackRegistered = false;

	// Short-name registry: maps known Values (e.g. type maps) to display names.
	private static List<Value> _shortNameKeys = new List<Value>();
	private static List<String> _shortNameVals = new List<String>();

	// Mark the Values this class owns.  Parameter defaults are created when an
	// intrinsic is *defined*, which is long before the first VM exists; without
	// this they are unreachable until EnsureBuilt copies them into a FuncDef and
	// roots the funcref, and any collection in that window frees them out from
	// under us.  Registered on first use (see EnsureMarkCallback).
	public static void MarkRoots(object user_data) {
		for (Int32 i = 0; i < _all.Count; i++) {
			List<Value> defaults = _all[i]._paramDefaults;
			for (Int32 j = 0; j < defaults.Count; j++) GCManager.Mark(defaults[j]);
		}
		for (Int32 i = 0; i < _shortNameKeys.Count; i++) GCManager.Mark(_shortNameKeys[i]);
	}

	private static void EnsureMarkCallback() {
		if (_markCallbackRegistered) return;
		_markCallbackRegistered = true;
		GCManager.RegisterMarkCallback(MarkRoots, null); // CPP: GCManager::RegisterMarkCallback(Intrinsic::MarkRoots, nullptr);
	}

	public static void AddShortName(Value v, String name) {
		EnsureMarkCallback();
		_shortNameKeys.Add(v);
		_shortNameVals.Add(name);
	}

	public static void ClearShortNames() {
		_shortNameKeys.Clear();
		_shortNameVals.Clear();
	}

	public static String GetShortName(Value v) {
		for (Int32 i = 0; i < _shortNameKeys.Count; i++) {
			if (_shortNameKeys[i].RefEquals(v)) return _shortNameVals[i];
		}
		return null;
	}

	public Intrinsic() {}

	// Return the number of intrinsics (initializing them if needed).
	public static Int32 Count() {
		if (!_initialized) {
			CoreIntrinsics.Init();
			_initialized = true;
		}
		return _all.Count;
	}

	public static Intrinsic Create(String name) {
		EnsureMarkCallback();
		Intrinsic result = new Intrinsic();
		result.Name = name;
		result._paramNames = new List<String>();
		result._paramDefaults = new List<Value>();
		_all.Add(result);
		_byName[name] = result;
		return result;
	}

	public void AddParam(String name) {
		_paramNames.Add(name);
		_paramDefaults.Add(Value.Null);
	}

	public void AddParam(String name, Value defaultValue) {
		// One default Value is shared by every call, so a list or map default
		// must be frozen, just as script-defined defaults are.
		defaultValue.Freeze();
		_paramNames.Add(name);
		_paramDefaults.Add(defaultValue);
	}

	// Declare a parameter that accepts an error argument.  By default an error
	// never reaches an intrinsic (see FuncDef.AcceptsErrorMask).  Accept one only
	// where the intrinsic has a real use for it -- showing it, storing it, or
	// searching for it -- and never lets it vanish silently.
	public void AddParam(String name, Value defaultValue, Boolean acceptsError) {
		if (acceptsError && _paramNames.Count < 32) _acceptsErrorMask |= (1u << _paramNames.Count);
		AddParam(name, defaultValue);
	}

	public static Intrinsic GetByName(String name) {
		Intrinsic result;
		if (_byName.TryGetValue(name, out result)) return result;
		return null;
	}

	public static Intrinsic GetByIndex(Int32 i) {
		return _all[i];
	}

	// Build (once) this intrinsic's FuncDef and a stable funcref Value.
	// The funcref is added as a permanent GC root: intrinsics live for the
	// lifetime of the process and are shared across VMs and resets.
	private void EnsureBuilt() {
		if (_funcDef == null) {
			_funcDef = BuildFuncDef();
			_funcRef = Value.make_funcref(_funcDef, Value.Null);
			GCManager.AddRoot(_funcRef);
		}
	}

	public Value GetFunc() {
		EnsureBuilt();
		return _funcRef;
	}

	// Build a FuncDef from this intrinsic's definition.
	public FuncDef BuildFuncDef() {
		FuncDef def = new FuncDef();
		def.Name = Name;
		for (Int32 i = 0; i < _paramNames.Count; i++) {
			def.ParamNames.Add(Value.make_string(_paramNames[i]));
			def.ParamDefaults.Add(_paramDefaults[i]);
		}
		def.MaxRegs = (UInt16)(_paramNames.Count + 1); // r0 + params
		def.NativeCallback = Code;
		def.AcceptsErrorMask = _acceptsErrorMask;
		def.AffectsState = AffectsState;
		return def;
	}

	// Populate the VM's intrinsics name->funcref table.  Intrinsic FuncDefs and
	// their funcref Values are built once (lazily) and shared across all VMs.
	public static void RegisterAll(Dictionary<String, Value> intrinsics) {
		if (!_initialized) {
			CoreIntrinsics.Init();
			_initialized = true;
		}
		intrinsics.Clear();
		for (Int32 i = 0; i < _all.Count; i++) {
			Intrinsic intr = _all[i];
			intr.EnsureBuilt();
			intrinsics[intr.Name] = intr._funcRef;
		}
		// Note: do NOT invalidate the cached type maps here.  They are GC roots
		// (CoreIntrinsics.MarkRoots), so they are never swept out from under us,
		// and they are built lazily -- so on the first call there is nothing to
		// rebuild anyway.  Doing it per VM would discard whatever a script has
		// added to `list`, `string` or `map`, process-wide, since those maps are
		// shared by every VM; it would also clear short names a host registered
		// during its own setup.
	}
}

}
