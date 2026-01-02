// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Assembler.cs

#pragma once
#include "core_includes.h"


namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct AcceptException;
class AcceptExceptionStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS















class AssemblerStorage : public std::enable_shared_from_this<AssemblerStorage> {
	public: List<FuncDef> Functions; // all functions
	public: FuncDef Current; // function we are currently building
	private: List<String> _labelNames; // label names within current function
	private: List<Int32> _labelAddresses; // corresponding instruction addresses within current function
	public: AssemblerStorage();
}; // end of class AssemblerStorage

struct Assembler {
	protected: std::shared_ptr<AssemblerStorage> storage;
  public:
	Assembler(std::shared_ptr<AssemblerStorage> stor) : storage(stor) {}
	Assembler() : storage(nullptr) {}
	friend bool IsNull(Assembler inst) { return inst.storage == nullptr; }
	private: AssemblerStorage* get() { return static_cast<AssemblerStorage*>(storage.get()); }

	public: List<FuncDef> Functions() { return get()->Functions; } // all functions
	public: void set_Functions(List<FuncDef> _v) { get()->Functions = _v; } // all functions
	public: FuncDef Current() { return get()->Current; } // function we are currently building
	public: void set_Current(FuncDef _v) { get()->Current = _v; } // function we are currently building
	private: List<String> _labelNames() { return get()->_labelNames; } // label names within current function
	private: void set__labelNames(List<String> _v) { get()->_labelNames = _v; } // label names within current function
	private: List<Int32> _labelAddresses() { return get()->_labelAddresses; } // corresponding instruction addresses within current function
	private: void set__labelAddresses(List<Int32> _v) { get()->_labelAddresses = _v; } // corresponding instruction addresses within current function
	public: Assembler() : Assembler(std::make_shared<AssemblerStorage>()) {}
}; // end of struct Assembler


// INLINE METHODS














