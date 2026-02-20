// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Intrinsic.cs

#pragma once
#include "core_includes.h"
// Intrinsic.cs - The Intrinsic class: a built-in function registered as a callable FuncRef.
// Each intrinsic is defined with a builder-style API:
//   f = Intrinsic.Create("name");
//   f.AddParam("paramName", defaultValue);
//   f.Code = (stk, bi, ac) => { ... };

#include "value.h"
#include "FuncDef.g.h"

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
struct SelfParselet;
class SelfParseletStorage;
struct SuperParselet;
class SuperParseletStorage;
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
struct Intrinsic;
class IntrinsicStorage;
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
struct SliceNode;
class SliceNodeStorage;
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
struct SelfNode;
class SelfNodeStorage;
struct SuperNode;
class SuperNodeStorage;
struct ReturnNode;
class ReturnNodeStorage;

// DECLARATIONS

class IntrinsicStorage : public std::enable_shared_from_this<IntrinsicStorage> {
	friend struct Intrinsic;
	public: String Name;
	public: NativeCallbackDelegate Code;
	private: List<String> _paramNames;
	private: List<Value> _paramDefaults;
	private: static List<Intrinsic> _all;
	private: static Boolean _initialized;
	public: IntrinsicStorage() {}

	

	public: static Intrinsic Create(String name);

	public: void AddParam(String name);

	public: void AddParam(String name, Value defaultValue);

	// Build a FuncDef from this intrinsic's definition.
	public: FuncDef BuildFuncDef();

	// Register all intrinsics into the VM's function list and intrinsics table.
	// Called by VM.Reset() after user functions are loaded.
	public: static void RegisterAll(List<FuncDef> functions, Dictionary<String, Value> intrinsics);
}; // end of class IntrinsicStorage

struct Intrinsic {
	friend class IntrinsicStorage;
	protected: std::shared_ptr<IntrinsicStorage> storage;
  public:
	Intrinsic(std::shared_ptr<IntrinsicStorage> stor) : storage(stor) {}
	Intrinsic() : storage(nullptr) {}
	Intrinsic(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Intrinsic& inst) { return inst.storage == nullptr; }
	private: IntrinsicStorage* get() const;

	public: String Name();
	public: void set_Name(String _v);
	public: NativeCallbackDelegate Code();
	public: void set_Code(NativeCallbackDelegate _v);
	private: List<String> _paramNames();
	private: void set__paramNames(List<String> _v);
	private: List<Value> _paramDefaults();
	private: void set__paramDefaults(List<Value> _v);
	private: List<Intrinsic> _all();
	private: void set__all(List<Intrinsic> _v);
	private: Boolean _initialized();
	private: void set__initialized(Boolean _v);
	public: static Intrinsic New() {
		return Intrinsic(std::make_shared<IntrinsicStorage>());
	}

	

	public: static Intrinsic Create(String name) { return IntrinsicStorage::Create(name); }

	public: inline void AddParam(String name);

	public: inline void AddParam(String name, Value defaultValue);

	// Build a FuncDef from this intrinsic's definition.
	public: inline FuncDef BuildFuncDef();

	// Register all intrinsics into the VM's function list and intrinsics table.
	// Called by VM.Reset() after user functions are loaded.
	public: static void RegisterAll(List<FuncDef> functions, Dictionary<String, Value> intrinsics) { return IntrinsicStorage::RegisterAll(functions, intrinsics); }
}; // end of struct Intrinsic

// INLINE METHODS

inline IntrinsicStorage* Intrinsic::get() const { return static_cast<IntrinsicStorage*>(storage.get()); }
inline String Intrinsic::Name() { return get()->Name; }
inline void Intrinsic::set_Name(String _v) { get()->Name = _v; }
inline NativeCallbackDelegate Intrinsic::Code() { return get()->Code; }
inline void Intrinsic::set_Code(NativeCallbackDelegate _v) { get()->Code = _v; }
inline List<String> Intrinsic::_paramNames() { return get()->_paramNames; }
inline void Intrinsic::set__paramNames(List<String> _v) { get()->_paramNames = _v; }
inline List<Value> Intrinsic::_paramDefaults() { return get()->_paramDefaults; }
inline void Intrinsic::set__paramDefaults(List<Value> _v) { get()->_paramDefaults = _v; }
inline List<Intrinsic> Intrinsic::_all() { return get()->_all; }
inline void Intrinsic::set__all(List<Intrinsic> _v) { get()->_all = _v; }
inline Boolean Intrinsic::_initialized() { return get()->_initialized; }
inline void Intrinsic::set__initialized(Boolean _v) { get()->_initialized = _v; }
inline void Intrinsic::AddParam(String name) { return get()->AddParam(name); }
inline void Intrinsic::AddParam(String name,Value defaultValue) { return get()->AddParam(name, defaultValue); }
inline FuncDef Intrinsic::BuildFuncDef() { return get()->BuildFuncDef(); }

} // end of namespace MiniScript
