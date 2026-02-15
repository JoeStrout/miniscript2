// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#pragma once
#include "core_includes.h"
#include "value.h"
#include "StringUtils.g.h"

namespace MiniScript {

// FORWARD DECLARATIONS

struct CodeGenerator;
class CodeGeneratorStorage;
struct VM;
class VMStorage;
struct CodeEmitterBase;
class CodeEmitterBaseStorage;
struct BytecodeEmitter;
class BytecodeEmitterStorage;
struct AssemblyEmitter;
class AssemblyEmitterStorage;
struct Assembler;
class AssemblerStorage;
struct Parselet;
class ParseletStorage;
struct PrefixParselet;
class PrefixParseletStorage;
struct InfixParselet;
class InfixParseletStorage;
struct NumberParselet;
class NumberParseletStorage;
struct StringParselet;
class StringParseletStorage;
struct IdentifierParselet;
class IdentifierParseletStorage;
struct UnaryOpParselet;
class UnaryOpParseletStorage;
struct GroupParselet;
class GroupParseletStorage;
struct ListParselet;
class ListParseletStorage;
struct MapParselet;
class MapParseletStorage;
struct BinaryOpParselet;
class BinaryOpParseletStorage;
struct CallParselet;
class CallParseletStorage;
struct IndexParselet;
class IndexParseletStorage;
struct MemberParselet;
class MemberParseletStorage;
struct Parser;
class ParserStorage;
struct FuncDef;
class FuncDefStorage;
struct ASTNode;
class ASTNodeStorage;
struct NumberNode;
class NumberNodeStorage;
struct StringNode;
class StringNodeStorage;
struct IdentifierNode;
class IdentifierNodeStorage;
struct AssignmentNode;
class AssignmentNodeStorage;
struct IndexedAssignmentNode;
class IndexedAssignmentNodeStorage;
struct UnaryOpNode;
class UnaryOpNodeStorage;
struct BinaryOpNode;
class BinaryOpNodeStorage;
struct CallNode;
class CallNodeStorage;
struct GroupNode;
class GroupNodeStorage;
struct ListNode;
class ListNodeStorage;
struct MapNode;
class MapNodeStorage;
struct IndexNode;
class IndexNodeStorage;
struct MemberNode;
class MemberNodeStorage;
struct MethodCallNode;
class MethodCallNodeStorage;
struct ExprCallNode;
class ExprCallNodeStorage;
struct WhileNode;
class WhileNodeStorage;
struct IfNode;
class IfNodeStorage;
struct ForNode;
class ForNodeStorage;
struct BreakNode;
class BreakNodeStorage;
struct ContinueNode;
class ContinueNodeStorage;
struct FunctionNode;
class FunctionNodeStorage;
struct ReturnNode;
class ReturnNodeStorage;

// DECLARATIONS

































































class FuncDefStorage : public std::enable_shared_from_this<FuncDefStorage> {
	friend struct FuncDef;
	public: String Name = "";
	public: List<UInt32> Code = List<UInt32>::New();
	public: List<Value> Constants = List<Value>::New();
	public: UInt16 MaxRegs = 0; // how many registers to reserve for this function
	public: List<Value> ParamNames = List<Value>::New(); // parameter names (as Value strings)
	public: List<Value> ParamDefaults = List<Value>::New(); // default values for parameters

	public: FuncDefStorage();

	public: void ReserveRegister(Int32 registerNumber);

	// Returns a string like "functionName(a, b=1, c=0)"
	public: String ToString();

	// Conversion to bool: returns true if function has a name
	public: operator bool() const;
	
	// Here is a comment at the end of the class.
	// Dunno why, but I guess the author had some things to say.
}; // end of class FuncDefStorage


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

	public: static FuncDef New() {
		return FuncDef(std::make_shared<FuncDefStorage>());
	}

	public: void ReserveRegister(Int32 registerNumber) { return get()->ReserveRegister(registerNumber); }

	// Returns a string like "functionName(a, b=1, c=0)"
	public: String ToString() { return get()->ToString(); }

	// Conversion to bool: returns true if function has a name
	public: operator bool() const { return (bool)(*get()); }
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
inline FuncDefStorage::operator bool() const {
	return Name != "";
}

} // end of namespace MiniScript
