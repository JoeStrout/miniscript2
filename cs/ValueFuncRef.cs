//*** BEGIN CS_ONLY ***
using System;
using static MiniScript.ValueHelpers;

namespace MiniScript {
	/// <summary>
	/// ValueFuncRef represents a function reference with closure support.
	/// It contains both the function definition index and the captured
	/// outer variable context (VarMap) from where the function was defined.
	/// </summary>
	public class ValueFuncRef {
		/// <summary>
		/// Index into the VM's functions array identifying the FuncDef
		/// </summary>
		public Int32 FuncIndex { get; set; }

		/// <summary>
		/// VarMap containing the captured outer variables from the defining scope.
		/// This is null for functions that don't capture any outer variables.
		/// </summary>
		public Value OuterVars { get; set; }

		public ValueFuncRef(Int32 funcIndex) {
			FuncIndex = funcIndex;
			OuterVars = make_null();
		}

		public ValueFuncRef(Int32 funcIndex, Value outerVars) {
			FuncIndex = funcIndex;
			OuterVars = outerVars;
		}

		public override string ToString() {
			if (is_null(OuterVars)) {
				return $"FuncRef({FuncIndex})";
			} else {
				return $"FuncRef({FuncIndex}, closure)";
			}
		}
	}
}
//*** END CS_ONLY ***