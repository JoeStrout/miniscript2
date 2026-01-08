// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#pragma once
#include "core_includes.h"
#include "value.h"
#include "StringUtils.g.h"

namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS












// Function definition: code, constants, and how many registers it needs



class FuncDefStorage : public std::enable_shared_from_this<FuncDefStorage> {
	public: String Name = "";
	public: List<UInt32> Code;
	public: List<Value> Constants;
	public: UInt16 MaxRegs = 0; // how many registers to reserve for this function
	public: List<Value> ParamNames; // parameter names (as Value strings)
	public: List<Value> ParamDefaults; // default values for parameters

	public: void ReserveRegister(Int32 registerNumber);

	// Returns a string like "functionName(a, b=1, c=0)"
	public: String ToString();

	// Conversion to bool: returns true if function has a name
	public: operator bool() const;
	
	// Here is a comment at the end of the class.
	// Dunno why, but I guess the author had some things to say.
}; // end of class FuncDefStorage

struct FuncDef {
	protected: std::shared_ptr<FuncDefStorage> storage;
  public:
	FuncDef(std::shared_ptr<FuncDefStorage> stor) : storage(stor) {}
	FuncDef() : storage(nullptr) {}
	friend bool IsNull(FuncDef inst) { return inst.storage == nullptr; }
	private: FuncDefStorage* get() const { return static_cast<FuncDefStorage*>(storage.get()); }

	public: String Name() { return get()->Name; }
	public: void set_Name(String _v) { get()->Name = _v; }
	public: List<UInt32> Code() { return get()->Code; }
	public: void set_Code(List<UInt32> _v) { get()->Code = _v; }
	public: List<Value> Constants() { return get()->Constants; }
	public: void set_Constants(List<Value> _v) { get()->Constants = _v; }
	public: UInt16 MaxRegs() { return get()->MaxRegs; } // how many registers to reserve for this function
	public: void set_MaxRegs(UInt16 _v) { get()->MaxRegs = _v; } // how many registers to reserve for this function
	public: List<Value> ParamNames() { return get()->ParamNames; } // parameter names (as Value strings)
	public: void set_ParamNames(List<Value> _v) { get()->ParamNames = _v; } // parameter names (as Value strings)
	public: List<Value> ParamDefaults() { return get()->ParamDefaults; } // default values for parameters
	public: void set_ParamDefaults(List<Value> _v) { get()->ParamDefaults = _v; } // default values for parameters

	public: void ReserveRegister(Int32 registerNumber) { return get()->ReserveRegister(registerNumber); }

	// Returns a string like "functionName(a, b=1, c=0)"
	public: String ToString() { return get()->ToString(); }

	// Conversion to bool: returns true if function has a name
	public: operator bool() const { return (bool)(*get()); }
}; // end of struct FuncDef


// INLINE METHODS

inline FuncDefStorage::operator bool() const {
	return Name != "";
}

} // end of namespace MiniScript
