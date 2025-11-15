// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#include "FuncDef.g.h"

namespace MiniScript {

	// Function definition: code, constants, and how many registers it needs

		void FuncDef::ReserveRegister(Int32 registerNumber) {
			UInt16 impliedCount = (UInt16)(registerNumber + 1);
			if (MaxRegs < impliedCount) MaxRegs = impliedCount;
		}

		// Returns a string like "functionName(a, b=1, c=0)"
		String FuncDef::ToString() {
			String result = Name + "(";
			for (Int32 i = 0; i < ParamNames.Count(); i++) {
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

}
