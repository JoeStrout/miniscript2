using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// CPP: #include "value.h"
// CPP: #include "StringUtils.g.h"

namespace MiniScript {

	// Function definition: code, constants, and how many registers it needs
	public class FuncDef {
		public String Name = "";
		public List<UInt32> Code = new List<UInt32>();
		public List<Value> Constants = new List<Value>();
		public UInt16 MaxRegs = 0; // how many registers to reserve for this function
		public List<Value> ParamNames = new List<Value>();     // parameter names (as Value strings)
		public List<Value> ParamDefaults = new List<Value>();  // default values for parameters

		public void ReserveRegister(Int32 registerNumber) {
			UInt16 impliedCount = (UInt16)(registerNumber + 1);
			if (MaxRegs < impliedCount) MaxRegs = impliedCount;
		}

		// Returns a string like "functionName(a, b=1, c=0)"
		public override String ToString() {
			String result = Name + "(";
			for (Int32 i = 0; i < ParamNames.Count; i++) {
				if (i > 0) result += ", ";
				result += as_cstring(ParamNames[i]);
				Value defaultVal = ParamDefaults[i];
				if (!is_null(defaultVal)) {
					result += "=";
					result += as_cstring(value_repr(defaultVal));
				}
			}
			result += ")";
			return result;
		}

		// Conversion to bool: returns true if function has a name
		/*** BEGIN H_ONLY ***
		public: operator bool() { return Name != ""; }
		*** END H_ONLY ***/
		//*** BEGIN CS_ONLY ***
		public static implicit operator bool(FuncDef funcDef) {
			return funcDef != null && !String.IsNullOrEmpty(funcDef.Name);
		}
		//*** END CS_ONLY ***
	}

}
