//*** BEGIN CS_ONLY ***
using System;

namespace MiniScript {
	// 
	// ValueFuncRef represents a function reference with closure support.
	// It contains both the function definition index and the captured
	// outer variable context (VarMap) from where the function was defined.
	// 
	public class ValueFuncRef {
		// 
		// Index into the VM's functions array identifying the FuncDef
		// 
		public Int32 FuncIndex { get; set; }

		// 
		// VarMap containing the captured outer variables from the defining scope.
		// This is null for functions that don't capture any outer variables.
		// 
		public Value OuterVars { get; set; }

		public ValueFuncRef(Int32 funcIndex) {
			FuncIndex = funcIndex;
			OuterVars = Value.Null;
		}

		public ValueFuncRef(Int32 funcIndex, Value outerVars) {
			FuncIndex = funcIndex;
			OuterVars = outerVars;
		}

		public override string ToString() {
			if (OuterVars.IsNull()) {
				return $"FuncRef(#{FuncIndex})";
			} else {
				return $"FuncRef(#{FuncIndex}, closure)";
			}
		}
	}
}
//*** END CS_ONLY ***