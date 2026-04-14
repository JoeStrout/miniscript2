// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "value.h"

namespace MiniScript {
struct Context;  // forward declaration; defined in VM.g.h
struct IntrinsicResult;  // forward declaration
typedef IntrinsicResult (*NativeCallbackDelegate)(Context, IntrinsicResult);
inline bool IsNull(NativeCallbackDelegate f) { return f == nullptr; }

// DECLARATIONS

class FuncDefStorage : public std::enable_shared_from_this<FuncDefStorage> {
	friend struct FuncDef;
	public: String Name = "";
	public: List<UInt32> Code = List<UInt32>::New();
	public: List<Value> Constants = List<Value>::New();
	public: UInt16 MaxRegs = 0; // how many registers to reserve for this function
	public: List<Value> ParamNames = List<Value>::New(); // parameter names (as Value strings)
	public: List<Value> ParamDefaults = List<Value>::New(); // default values for parameters
	public: Int16 SelfReg = -1; // register for 'self' (-1 if not used)
	public: Int16 SuperReg = -1; // register for 'super' (-1 if not used)
	public: String Note = "";
	public: String SourceLoc = "";
	public: String FileName = "";
	private: List<Int32> _lineRLEPC = List<Int32>::New();
	private: List<Int32> _lineRLELine = List<Int32>::New();

	// RLE line-number table: _lineRLEPC[i] is the first bytecode PC whose source
	// line is _lineRLELine[i].  The run continues until the next entry.
	// Use AddInstruction (not Code.Add) when building bytecode so that this
	// table is kept in sync.

	// Append one bytecode instruction together with its source line number.
	// Call this instead of Code.Add so the RLE line table is maintained.
	public: void AddInstruction(UInt32 instruction, Int32 lineNumber);

	// Return the source line number for the instruction at the given PC index.
	// Returns 0 if no line information is available.
	public: Int32 GetLineNumber(Int32 pc);
	public: NativeCallbackDelegate NativeCallback = null;

	// Native callback for intrinsic functions. When non-null, this FuncDef
	// represents a built-in function: CALL invokes the callback directly
	// instead of executing bytecode.  Parameters are in stack[baseIndex+1..].

	public: FuncDefStorage();

	public: void ReserveRegister(Int32 registerNumber);

	// Returns a string like "functionName(a, b=1, c=0)"
	public: String ToString();

	// Conversion to bool: returns true if function has a name
	public: operator bool() const;
	
}; // end of class FuncDefStorage

// Native callback for intrinsic functions.

// Function definition: code, constants, and how many registers it needs
struct FuncDef {
	friend class FuncDefStorage;
	protected: std::shared_ptr<FuncDefStorage> storage;
  public:
	FuncDef(std::shared_ptr<FuncDefStorage> stor) : storage(stor) {}
	FuncDef() : storage(nullptr) {}
	FuncDef(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const FuncDef& inst) { return inst.storage == nullptr; }
	private: FuncDefStorage* get() const;

	public: String Name();
	public: void set_Name(String _v);
	public: List<UInt32> Code();
	public: void set_Code(List<UInt32> _v);
	public: List<Value> Constants();
	public: void set_Constants(List<Value> _v);
	public: UInt16 MaxRegs(); // how many registers to reserve for this function
	public: void set_MaxRegs(UInt16 _v); // how many registers to reserve for this function
	public: List<Value> ParamNames(); // parameter names (as Value strings)
	public: void set_ParamNames(List<Value> _v); // parameter names (as Value strings)
	public: List<Value> ParamDefaults(); // default values for parameters
	public: void set_ParamDefaults(List<Value> _v); // default values for parameters
	public: Int16 SelfReg(); // register for 'self' (-1 if not used)
	public: void set_SelfReg(Int16 _v); // register for 'self' (-1 if not used)
	public: Int16 SuperReg(); // register for 'super' (-1 if not used)
	public: void set_SuperReg(Int16 _v); // register for 'super' (-1 if not used)
	public: String Note();
	public: void set_Note(String _v);
	public: String SourceLoc();
	public: void set_SourceLoc(String _v);
	public: String FileName();
	public: void set_FileName(String _v);
	private: List<Int32> _lineRLEPC();
	private: void set__lineRLEPC(List<Int32> _v);
	private: List<Int32> _lineRLELine();
	private: void set__lineRLELine(List<Int32> _v);

	// RLE line-number table: _lineRLEPC[i] is the first bytecode PC whose source
	// line is _lineRLELine[i].  The run continues until the next entry.
	// Use AddInstruction (not Code.Add) when building bytecode so that this
	// table is kept in sync.

	// Append one bytecode instruction together with its source line number.
	// Call this instead of Code.Add so the RLE line table is maintained.
	public: inline void AddInstruction(UInt32 instruction, Int32 lineNumber);

	// Return the source line number for the instruction at the given PC index.
	// Returns 0 if no line information is available.
	public: inline Int32 GetLineNumber(Int32 pc);
	public: NativeCallbackDelegate NativeCallback();
	public: void set_NativeCallback(NativeCallbackDelegate _v);

	// Native callback for intrinsic functions. When non-null, this FuncDef
	// represents a built-in function: CALL invokes the callback directly
	// instead of executing bytecode.  Parameters are in stack[baseIndex+1..].

	public: static FuncDef New() {
		return FuncDef(std::make_shared<FuncDefStorage>());
	}

	public: inline void ReserveRegister(Int32 registerNumber);

	// Returns a string like "functionName(a, b=1, c=0)"
	public: String ToString() { return get()->ToString(); }

	// Conversion to bool: returns true if function has a name
	public: operator bool() const { return (bool)(*get()); }
	public: FuncDefStorage* get_storage() const { return storage.get(); }
}; // end of struct FuncDef

// INLINE METHODS

inline FuncDefStorage* FuncDef::get() const { return static_cast<FuncDefStorage*>(storage.get()); }
inline String FuncDef::Name() { return get()->Name; }
inline void FuncDef::set_Name(String _v) { get()->Name = _v; }
inline List<UInt32> FuncDef::Code() { return get()->Code; }
inline void FuncDef::set_Code(List<UInt32> _v) { get()->Code = _v; }
inline List<Value> FuncDef::Constants() { return get()->Constants; }
inline void FuncDef::set_Constants(List<Value> _v) { get()->Constants = _v; }
inline UInt16 FuncDef::MaxRegs() { return get()->MaxRegs; } // how many registers to reserve for this function
inline void FuncDef::set_MaxRegs(UInt16 _v) { get()->MaxRegs = _v; } // how many registers to reserve for this function
inline List<Value> FuncDef::ParamNames() { return get()->ParamNames; } // parameter names (as Value strings)
inline void FuncDef::set_ParamNames(List<Value> _v) { get()->ParamNames = _v; } // parameter names (as Value strings)
inline List<Value> FuncDef::ParamDefaults() { return get()->ParamDefaults; } // default values for parameters
inline void FuncDef::set_ParamDefaults(List<Value> _v) { get()->ParamDefaults = _v; } // default values for parameters
inline Int16 FuncDef::SelfReg() { return get()->SelfReg; } // register for 'self' (-1 if not used)
inline void FuncDef::set_SelfReg(Int16 _v) { get()->SelfReg = _v; } // register for 'self' (-1 if not used)
inline Int16 FuncDef::SuperReg() { return get()->SuperReg; } // register for 'super' (-1 if not used)
inline void FuncDef::set_SuperReg(Int16 _v) { get()->SuperReg = _v; } // register for 'super' (-1 if not used)
inline String FuncDef::Note() { return get()->Note; }
inline void FuncDef::set_Note(String _v) { get()->Note = _v; }
inline String FuncDef::SourceLoc() { return get()->SourceLoc; }
inline void FuncDef::set_SourceLoc(String _v) { get()->SourceLoc = _v; }
inline String FuncDef::FileName() { return get()->FileName; }
inline void FuncDef::set_FileName(String _v) { get()->FileName = _v; }
inline List<Int32> FuncDef::_lineRLEPC() { return get()->_lineRLEPC; }
inline void FuncDef::set__lineRLEPC(List<Int32> _v) { get()->_lineRLEPC = _v; }
inline List<Int32> FuncDef::_lineRLELine() { return get()->_lineRLELine; }
inline void FuncDef::set__lineRLELine(List<Int32> _v) { get()->_lineRLELine = _v; }
inline void FuncDef::AddInstruction(UInt32 instruction,Int32 lineNumber) { return get()->AddInstruction(instruction, lineNumber); }
inline Int32 FuncDef::GetLineNumber(Int32 pc) { return get()->GetLineNumber(pc); }
inline NativeCallbackDelegate FuncDef::NativeCallback() { return get()->NativeCallback; }
inline void FuncDef::set_NativeCallback(NativeCallbackDelegate _v) { get()->NativeCallback = _v; }
inline void FuncDef::ReserveRegister(Int32 registerNumber) { return get()->ReserveRegister(registerNumber); }
inline FuncDefStorage::operator bool() const {
	return Name != "";
}

} // end of namespace MiniScript
