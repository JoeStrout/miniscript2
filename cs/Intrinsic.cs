// Intrinsic.cs - The Intrinsic class: a built-in function registered as a callable FuncRef.
// Each intrinsic is defined with a builder-style API:
//   f = Intrinsic.Create("name");
//   f.AddParam("paramName", defaultValue);
//   f.Code = (stk, bi, ac) => { ... };

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "FuncDef.g.h"
// CPP: #include "CoreIntrinsics.g.h"

namespace MiniScript {

public class Intrinsic {
	public String Name;

	public NativeCallbackDelegate Code;

	private List<String> _paramNames;
	private List<Value> _paramDefaults;
	private FuncDef _funcDef = null;
	private Value _funcRef = val_null;

	private static List<Intrinsic> _all = new List<Intrinsic>();
	private static Boolean _initialized = false;

	// Short-name registry: maps known Values (e.g. type maps) to display names.
	private static List<Value> _shortNameKeys = new List<Value>();
	private static List<String> _shortNameVals = new List<String>();

	public static void AddShortName(Value v, String name) {
		_shortNameKeys.Add(v);
		_shortNameVals.Add(name);
	}

	public static void ClearShortNames() {
		_shortNameKeys.Clear();
		_shortNameVals.Clear();
	}

	public static String GetShortName(Value v) {
		for (Int32 i = 0; i < _shortNameKeys.Count; i++) {
			if (value_identical(_shortNameKeys[i], v)) return _shortNameVals[i];
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
		Intrinsic result = new Intrinsic();
		result.Name = name;
		result._paramNames = new List<String>();
		result._paramDefaults = new List<Value>();
		_all.Add(result);
		return result;
	}

	public void AddParam(String name) {
		_paramNames.Add(name);
		_paramDefaults.Add(val_null);
	}

	public void AddParam(String name, Value defaultValue) {
		_paramNames.Add(name);
		_paramDefaults.Add(defaultValue);
	}

	public static Intrinsic GetByName(String name) {
		for (Int32 i = 0; i < _all.Count; i++) {
			if (_all[i].Name == name) return _all[i];
		}
		return null;
	}

	public static Int32 AllCount() {	// ToDo: isn't this redundant with Count, above?
		return _all.Count;
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
			_funcRef = make_funcref(_funcDef, val_null);
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
			def.ParamNames.Add(make_string(_paramNames[i]));
			def.ParamDefaults.Add(_paramDefaults[i]);
		}
		def.MaxRegs = (UInt16)(_paramNames.Count + 1); // r0 + params
		def.NativeCallback = Code;
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
		// Rebuild cached type maps (they are GC objects that may have been swept).
		CoreIntrinsics.InvalidateTypeMaps();
	}
}

}
