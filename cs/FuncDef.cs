using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "StringUtils.g.h"
// CPP: #include "IntrinsicAPI.g.h"
// CPP: #include "gc.h"

namespace MiniScript {

// Native callback for intrinsic functions.
// H: struct Context;  // forward declaration; defined in VM.g.h
// H: struct IntrinsicResult;  // forward declaration
// H: typedef IntrinsicResult (*NativeCallbackDelegate)(Context, IntrinsicResult);
// H: inline bool IsNull(NativeCallbackDelegate f) { return f == nullptr; }
public delegate IntrinsicResult NativeCallbackDelegate(Context context, IntrinsicResult partialResult); // CPP:

// Function definition: code, constants, and how many registers it needs
public class FuncDef {
	public String Name = "";
	public List<UInt32> Code = new List<UInt32>();
	public List<Value> Constants = new List<Value>();
	public UInt16 MaxRegs = 0; // how many registers to reserve for this function
	public List<Value> ParamNames = new List<Value>();     // parameter names (as Value strings)
	public List<Value> ParamDefaults = new List<Value>();  // default values for parameters
	public Int16 SelfReg = -1;   // register for 'self' (-1 if not used)
	public Int16 SuperReg = -1;  // register for 'super' (-1 if not used)

	// Native callback for intrinsic functions. When non-null, this FuncDef
	// represents a built-in function: CALL invokes the callback directly
	// instead of executing bytecode.  Parameters are in stack[baseIndex+1..].
	public NativeCallbackDelegate NativeCallback = null;

	public FuncDef() {
	}

	public void ReserveRegister(Int32 registerNumber) {
		UInt16 impliedCount = (UInt16)(registerNumber + 1);
		if (MaxRegs < impliedCount) MaxRegs = impliedCount;
	}

	// Returns a string like "functionName(a, b=1, c=0)"
	public override String ToString() {
		String result = Name + "(";
		Value defaultVal;
		for (Int32 i = 0; i < ParamNames.Count; i++) {
			if (i > 0) result += ", ";
			result += as_cstring(ParamNames[i]);
			defaultVal = ParamDefaults[i];
			if (!is_null(defaultVal)) {
				result += "=";
				result += as_cstring(value_repr(defaultVal));
			}
		}
		result += ")";
		return result;
	}

	// Conversion to bool: returns true if function has a name
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static implicit operator bool(FuncDef funcDef) {
		return funcDef != null && !String.IsNullOrEmpty(funcDef.Name); // CPP: return Name != "";
	}
	
	// H_WRAPPER: public: FuncDefStorage* get_storage() const { return storage.get(); }
}

}
