// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#ifndef __FUNCDEF_H
#define __FUNCDEF_H

#include "core_includes.h"
#include "value.h"
#include "StringUtils.g.h"

namespace MiniScript {

	// Function definition: code, constants, and how many registers it needs
	class FuncDef {
		public: String Name = "";
		public: List<UInt32> Code = List<UInt32>();
		public: List<Value> Constants = List<Value>();
		public: UInt16 MaxRegs = 0; // how many registers to reserve for this function
		public: List<Value> ParamNames = List<Value>();     // parameter names (as Value strings)
		public: List<Value> ParamDefaults = List<Value>();  // default values for parameters

		public: void ReserveRegister(Int32 registerNumber);

		// Returns a string like "functionName(a, b=1, c=0)"
		public: String ToString();

		// Conversion to bool: returns true if function has a name
		public: operator bool() { return Name != ""; }
	}; // end of class FuncDef

}

#endif // __FUNCDEF_H
