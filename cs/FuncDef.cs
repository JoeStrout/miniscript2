using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "StringUtils.g.h"
// CPP: #include "gc.h"

namespace MiniScript {

// Function definition: code, constants, and how many registers it needs
public class FuncDef {
	public String Name = "";
	public List<UInt32> Code = new List<UInt32>();
	public List<Value> Constants = new List<Value>();
	public UInt16 MaxRegs = 0; // how many registers to reserve for this function
	public List<Value> ParamNames = new List<Value>();     // parameter names (as Value strings)
	public List<Value> ParamDefaults = new List<Value>();  // default values for parameters

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
	
	// Here is a comment at the end of the class.
	// Dunno why, but I guess the author had some things to say.
}

}
