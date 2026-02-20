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

	private static List<Intrinsic> _all = new List<Intrinsic>();
	private static Boolean _initialized = false;

	public Intrinsic() {}

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
		_paramDefaults.Add(make_null());
	}

	public void AddParam(String name, Value defaultValue) {
		_paramNames.Add(name);
		_paramDefaults.Add(defaultValue);
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

	// Register all intrinsics into the VM's function list and intrinsics table.
	// Called by VM.Reset() after user functions are loaded.
	public static void RegisterAll(List<FuncDef> functions, Dictionary<String, Value> intrinsics) {
		if (!_initialized) {
			CoreIntrinsics.Init();
			_initialized = true;
		}
		intrinsics.Clear();
		for (Int32 i = 0; i < _all.Count; i++) {
			Intrinsic intr = _all[i];
			FuncDef def = intr.BuildFuncDef();
			Int32 funcIndex = functions.Count;
			functions.Add(def);
			intrinsics[intr.Name] = make_funcref(funcIndex, val_null);
		}
	}
}

}
