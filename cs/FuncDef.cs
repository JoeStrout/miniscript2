using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "value.h"
// H: #include <functional>
// CPP: #include "StringUtils.g.h"
// CPP: #include "IntrinsicAPI.g.h"

namespace MiniScript {

// Native callback for intrinsic functions.
// H: struct Context;  // forward declaration; defined in VM.g.h
// H: struct IntrinsicResult;  // forward declaration
// H: typedef std::function<IntrinsicResult(Context, IntrinsicResult)> NativeCallbackDelegate;
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
	public String Note = "";
	public String SourceLoc = "";
	public String FileName = "";

	// ── Global-reference table ────────────────────────────────────────────────
	//
	// The names this function reaches as globals, in the order the BC operand of
	// GLOADC/GLOADV/GSTORE indexes them.  GlobalNames is filled at compile time
	// and never changes; GlobalSlots is its resolution into slots of some
	// particular Globals table, and is valid only while GlobalCacheId equals that
	// table's Id.  See notes/GLOBALS.md section 4.3 for why the cache is guarded
	// rather than baked in: the same FuncDef may be called against a different
	// namespace than the one it was compiled alongside, which is what makes
	// cross-interpreter seeding work.
	//
	// Id 0 is never issued (Globals pre-increments), so 0 means "never resolved".
	public List<Value> GlobalNames = new List<Value>();
	public List<Int32> GlobalSlots = new List<Int32>();
	public Int32 GlobalCacheId = 0;

	// Intern a name into the global-reference table, returning its index.  Used
	// by the code generator and the assembler while building this function.
	public Int32 AddGlobalRef(Value name) {
		for (Int32 i = 0; i < GlobalNames.Count; i++) {
			if (GlobalNames[i] == name) return i;
		}
		GlobalNames.Add(name);
		GlobalSlots.Add(-1);
		return GlobalNames.Count - 1;
	}

	// RLE line-number table: _lineRLEPC[i] is the first bytecode PC whose source
	// line is _lineRLELine[i].  The run continues until the next entry.
	// Use AddInstruction (not Code.Add) when building bytecode so that this
	// table is kept in sync.
	private List<Int32> _lineRLEPC = new List<Int32>();
	private List<Int32> _lineRLELine = new List<Int32>();

	// Append one bytecode instruction together with its source line number.
	// Call this instead of Code.Add so the RLE line table is maintained.
	public void AddInstruction(UInt32 instruction, Int32 lineNumber) {
		Code.Add(instruction);
		Int32 count = _lineRLELine.Count;
		if (count == 0 || _lineRLELine[count - 1] != lineNumber) {
			_lineRLEPC.Add(Code.Count - 1);
			_lineRLELine.Add(lineNumber);
		}
	}

	// Return the source line number for the instruction at the given PC index.
	// Returns 0 if no line information is available.
	public Int32 GetLineNumber(Int32 pc) {
		Int32 result = 0;
		for (Int32 i = 0; i < _lineRLEPC.Count; i++) {
			if (_lineRLEPC[i] > pc) break;
			result = _lineRLELine[i];
		}
		return result;
	}

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
			result += ParamNames[i].AsCString();
			defaultVal = ParamDefaults[i];
			if (!defaultVal.IsNull()) {
				result += "=";
				result += defaultVal.Repr(null).AsCString();
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
