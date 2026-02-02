// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeEmitter.cs

#pragma once
#include "core_includes.h"
// CodeEmitter.cs - Base class and implementations for emitting bytecode or assembly
// Provides an abstraction layer for code generation output.

#include "Bytecode.g.h"
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

// DECLARATIONS











// Tracks a pending label reference that needs patching
struct LabelRef {
	public: Int32 CodeIndex; // index in _code where the instruction is
	public: Int32 LabelId; // label being referenced
	public: Opcode Op; // opcode (for re-encoding)
	public: Int32 A; // A operand (for re-encoding)
}; // end of struct LabelRef














































// Abstract base class for emitting code (bytecode or assembly text)
struct CodeEmitterBase {
	protected: std::shared_ptr<CodeEmitterBaseStorage> storage;
  public:
	CodeEmitterBase(std::shared_ptr<CodeEmitterBaseStorage> stor) : storage(stor) {}
	CodeEmitterBase() : storage(nullptr) {}
	CodeEmitterBase(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const CodeEmitterBase& inst) { return inst.storage == nullptr; }
	private: CodeEmitterBaseStorage* get() const;
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(CodeEmitterBase inst) {
		auto stor = std::dynamic_pointer_cast<StorageType>(inst.storage);
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(stor); 
	}

	public: void Emit(Opcode op, String comment); // INS: opcode only
	public: void EmitA(Opcode op, Int32 a, String comment); // INS_A: 8-bit A field
	public: void EmitAB(Opcode op, Int32 a, Int32 bc, String comment); // INS_AB: 8-bit A + 16-bit BC
	public: void EmitBC(Opcode op, Int32 ab, Int32 c, String comment); // INS_BC: 16-bit AB + 8-bit C
	public: void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment); // INS_ABC: 8-bit A + 8-bit B + 8-bit C
	public: Int32 AddConstant(Value value);
	public: Int32 CreateLabel();
	public: void PlaceLabel(Int32 labelId);
	public: void EmitJump(Opcode op, Int32 labelId, String comment);
	public: void ReserveRegister(Int32 registerNumber);
	public: FuncDef Finalize(String name);
}; // end of struct CodeEmitterBase

template<typename WrapperType, typename StorageType> WrapperType As(CodeEmitterBase inst);

class CodeEmitterBaseStorage : public std::enable_shared_from_this<CodeEmitterBaseStorage> {
	friend struct CodeEmitterBase;
	public: virtual ~CodeEmitterBaseStorage() {}
	public: virtual void Emit(Opcode op, String comment) = 0; // INS: opcode only
	public: virtual void EmitA(Opcode op, Int32 a, String comment) = 0; // INS_A: 8-bit A field
	public: virtual void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) = 0; // INS_AB: 8-bit A + 16-bit BC
	public: virtual void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) = 0; // INS_BC: 16-bit AB + 8-bit C
	public: virtual void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) = 0; // INS_ABC: 8-bit A + 8-bit B + 8-bit C
	public: virtual Int32 AddConstant(Value value) = 0;
	public: virtual Int32 CreateLabel() = 0;
	public: virtual void PlaceLabel(Int32 labelId) = 0;
	public: virtual void EmitJump(Opcode op, Int32 labelId, String comment) = 0;
	public: virtual void ReserveRegister(Int32 registerNumber) = 0;
	public: virtual FuncDef Finalize(String name) = 0;
	// Emit instructions with varying operand patterns
	// Method names match BytecodeUtil.INS_* patterns

	// Add a constant to the constant pool, return its index

	// Label management for jumps

	// Track register usage

	// Finalize and return the compiled function
}; // end of class CodeEmitterBaseStorage

class BytecodeEmitterStorage : public CodeEmitterBaseStorage {
	friend struct BytecodeEmitter;
	private: List<UInt32> _code;
	private: List<Value> _constants;
	private: UInt16 _maxRegs;
	private: Dictionary<Int32, Int32> _labelAddresses; // labelId -> code address
	private: List<LabelRef> _labelRefs; // pending label references
	private: Int32 _nextLabelId;

	public: BytecodeEmitterStorage();

	public: void Emit(Opcode op, String comment);

	public: void EmitA(Opcode op, Int32 a, String comment);

	public: void EmitAB(Opcode op, Int32 a, Int32 bc, String comment);

	public: void EmitBC(Opcode op, Int32 ab, Int32 c, String comment);

	public: void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment);

	public: Int32 AddConstant(Value value);

	public: Int32 CreateLabel();

	public: void PlaceLabel(Int32 labelId);

	public: void EmitJump(Opcode op, Int32 labelId, String comment);

	public: void ReserveRegister(Int32 registerNumber);

	public: FuncDef Finalize(String name);
}; // end of class BytecodeEmitterStorage

class AssemblyEmitterStorage : public CodeEmitterBaseStorage {
	friend struct AssemblyEmitter;
	private: List<String> _lines;
	private: List<Value> _constants;
	private: UInt16 _maxRegs;
	private: Dictionary<Int32, String> _labelNames;
	private: Int32 _nextLabelId;

	public: AssemblyEmitterStorage();

	public: void Emit(Opcode op, String comment);

	public: void EmitA(Opcode op, Int32 a, String comment);

	public: void EmitAB(Opcode op, Int32 a, Int32 bc, String comment);

	public: void EmitBC(Opcode op, Int32 ab, Int32 c, String comment);

	public: void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment);

	public: Int32 AddConstant(Value value);

	public: Int32 CreateLabel();

	public: void PlaceLabel(Int32 labelId);

	public: void EmitJump(Opcode op, Int32 labelId, String comment);

	public: void ReserveRegister(Int32 registerNumber);

	public: FuncDef Finalize(String name);

	// Get the generated assembly text
	public: List<String> GetLines();

	// Get the assembly as a single string
	public: String GetAssembly();
}; // end of class AssemblyEmitterStorage



// Emits directly to bytecode (production use)
struct BytecodeEmitter : public CodeEmitterBase {
	BytecodeEmitter(std::shared_ptr<BytecodeEmitterStorage> stor);
	BytecodeEmitter() : CodeEmitterBase() {}
	BytecodeEmitter(std::nullptr_t) : CodeEmitterBase(nullptr) {}
	private: BytecodeEmitterStorage* get() const;

	private: List<UInt32> _code();
	private: void set__code(List<UInt32> _v);
	private: List<Value> _constants();
	private: void set__constants(List<Value> _v);
	private: UInt16 _maxRegs();
	private: void set__maxRegs(UInt16 _v);
	private: Dictionary<Int32, Int32> _labelAddresses(); // labelId -> code address
	private: void set__labelAddresses(Dictionary<Int32, Int32> _v); // labelId -> code address
	private: List<LabelRef> _labelRefs(); // pending label references
	private: void set__labelRefs(List<LabelRef> _v); // pending label references
	private: Int32 _nextLabelId();
	private: void set__nextLabelId(Int32 _v);

	public: static BytecodeEmitter New() {
		return BytecodeEmitter(std::make_shared<BytecodeEmitterStorage>());
	}

	public: void Emit(Opcode op, String comment) { return get()->Emit(op, comment); }

	public: void EmitA(Opcode op, Int32 a, String comment) { return get()->EmitA(op, a, comment); }

	public: void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) { return get()->EmitAB(op, a, bc, comment); }

	public: void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) { return get()->EmitBC(op, ab, c, comment); }

	public: void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) { return get()->EmitABC(op, a, b, c, comment); }

	public: Int32 AddConstant(Value value) { return get()->AddConstant(value); }

	public: Int32 CreateLabel() { return get()->CreateLabel(); }

	public: void PlaceLabel(Int32 labelId) { return get()->PlaceLabel(labelId); }

	public: void EmitJump(Opcode op, Int32 labelId, String comment) { return get()->EmitJump(op, labelId, comment); }

	public: void ReserveRegister(Int32 registerNumber) { return get()->ReserveRegister(registerNumber); }

	public: FuncDef Finalize(String name) { return get()->Finalize(name); }
}; // end of struct BytecodeEmitter


// Emits assembly text (for debugging and testing)
struct AssemblyEmitter : public CodeEmitterBase {
	AssemblyEmitter(std::shared_ptr<AssemblyEmitterStorage> stor);
	AssemblyEmitter() : CodeEmitterBase() {}
	AssemblyEmitter(std::nullptr_t) : CodeEmitterBase(nullptr) {}
	private: AssemblyEmitterStorage* get() const;

	private: List<String> _lines();
	private: void set__lines(List<String> _v);
	private: List<Value> _constants();
	private: void set__constants(List<Value> _v);
	private: UInt16 _maxRegs();
	private: void set__maxRegs(UInt16 _v);
	private: Dictionary<Int32, String> _labelNames();
	private: void set__labelNames(Dictionary<Int32, String> _v);
	private: Int32 _nextLabelId();
	private: void set__nextLabelId(Int32 _v);

	public: static AssemblyEmitter New() {
		return AssemblyEmitter(std::make_shared<AssemblyEmitterStorage>());
	}

	public: void Emit(Opcode op, String comment) { return get()->Emit(op, comment); }

	public: void EmitA(Opcode op, Int32 a, String comment) { return get()->EmitA(op, a, comment); }

	public: void EmitAB(Opcode op, Int32 a, Int32 bc, String comment) { return get()->EmitAB(op, a, bc, comment); }

	public: void EmitBC(Opcode op, Int32 ab, Int32 c, String comment) { return get()->EmitBC(op, ab, c, comment); }

	public: void EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) { return get()->EmitABC(op, a, b, c, comment); }

	public: Int32 AddConstant(Value value) { return get()->AddConstant(value); }

	public: Int32 CreateLabel() { return get()->CreateLabel(); }

	public: void PlaceLabel(Int32 labelId) { return get()->PlaceLabel(labelId); }

	public: void EmitJump(Opcode op, Int32 labelId, String comment) { return get()->EmitJump(op, labelId, comment); }

	public: void ReserveRegister(Int32 registerNumber) { return get()->ReserveRegister(registerNumber); }

	public: FuncDef Finalize(String name) { return get()->Finalize(name); }

	// Get the generated assembly text
	public: List<String> GetLines() { return get()->GetLines(); }

	// Get the assembly as a single string
	public: String GetAssembly() { return get()->GetAssembly(); }
}; // end of struct AssemblyEmitter


// INLINE METHODS

inline CodeEmitterBaseStorage* CodeEmitterBase::get() const { return static_cast<CodeEmitterBaseStorage*>(storage.get()); }
inline void CodeEmitterBase::Emit(Opcode op, String comment) { return get()->Emit(op, comment); } // INS: opcode only
inline void CodeEmitterBase::EmitA(Opcode op, Int32 a, String comment) { return get()->EmitA(op, a, comment); } // INS_A: 8-bit A field
inline void CodeEmitterBase::EmitAB(Opcode op, Int32 a, Int32 bc, String comment) { return get()->EmitAB(op, a, bc, comment); } // INS_AB: 8-bit A + 16-bit BC
inline void CodeEmitterBase::EmitBC(Opcode op, Int32 ab, Int32 c, String comment) { return get()->EmitBC(op, ab, c, comment); } // INS_BC: 16-bit AB + 8-bit C
inline void CodeEmitterBase::EmitABC(Opcode op, Int32 a, Int32 b, Int32 c, String comment) { return get()->EmitABC(op, a, b, c, comment); } // INS_ABC: 8-bit A + 8-bit B + 8-bit C
inline Int32 CodeEmitterBase::AddConstant(Value value) { return get()->AddConstant(value); }
inline Int32 CodeEmitterBase::CreateLabel() { return get()->CreateLabel(); }
inline void CodeEmitterBase::PlaceLabel(Int32 labelId) { return get()->PlaceLabel(labelId); }
inline void CodeEmitterBase::EmitJump(Opcode op, Int32 labelId, String comment) { return get()->EmitJump(op, labelId, comment); }
inline void CodeEmitterBase::ReserveRegister(Int32 registerNumber) { return get()->ReserveRegister(registerNumber); }
inline FuncDef CodeEmitterBase::Finalize(String name) { return get()->Finalize(name); }

inline BytecodeEmitter::BytecodeEmitter(std::shared_ptr<BytecodeEmitterStorage> stor) : CodeEmitterBase(stor) {}
inline BytecodeEmitterStorage* BytecodeEmitter::get() const { return static_cast<BytecodeEmitterStorage*>(storage.get()); }
inline List<UInt32> BytecodeEmitter::_code() { return get()->_code; }
inline void BytecodeEmitter::set__code(List<UInt32> _v) { get()->_code = _v; }
inline List<Value> BytecodeEmitter::_constants() { return get()->_constants; }
inline void BytecodeEmitter::set__constants(List<Value> _v) { get()->_constants = _v; }
inline UInt16 BytecodeEmitter::_maxRegs() { return get()->_maxRegs; }
inline void BytecodeEmitter::set__maxRegs(UInt16 _v) { get()->_maxRegs = _v; }
inline Dictionary<Int32, Int32> BytecodeEmitter::_labelAddresses() { return get()->_labelAddresses; } // labelId -> code address
inline void BytecodeEmitter::set__labelAddresses(Dictionary<Int32, Int32> _v) { get()->_labelAddresses = _v; } // labelId -> code address
inline List<LabelRef> BytecodeEmitter::_labelRefs() { return get()->_labelRefs; } // pending label references
inline void BytecodeEmitter::set__labelRefs(List<LabelRef> _v) { get()->_labelRefs = _v; } // pending label references
inline Int32 BytecodeEmitter::_nextLabelId() { return get()->_nextLabelId; }
inline void BytecodeEmitter::set__nextLabelId(Int32 _v) { get()->_nextLabelId = _v; }

inline AssemblyEmitter::AssemblyEmitter(std::shared_ptr<AssemblyEmitterStorage> stor) : CodeEmitterBase(stor) {}
inline AssemblyEmitterStorage* AssemblyEmitter::get() const { return static_cast<AssemblyEmitterStorage*>(storage.get()); }
inline List<String> AssemblyEmitter::_lines() { return get()->_lines; }
inline void AssemblyEmitter::set__lines(List<String> _v) { get()->_lines = _v; }
inline List<Value> AssemblyEmitter::_constants() { return get()->_constants; }
inline void AssemblyEmitter::set__constants(List<Value> _v) { get()->_constants = _v; }
inline UInt16 AssemblyEmitter::_maxRegs() { return get()->_maxRegs; }
inline void AssemblyEmitter::set__maxRegs(UInt16 _v) { get()->_maxRegs = _v; }
inline Dictionary<Int32, String> AssemblyEmitter::_labelNames() { return get()->_labelNames; }
inline void AssemblyEmitter::set__labelNames(Dictionary<Int32, String> _v) { get()->_labelNames = _v; }
inline Int32 AssemblyEmitter::_nextLabelId() { return get()->_nextLabelId; }
inline void AssemblyEmitter::set__nextLabelId(Int32 _v) { get()->_nextLabelId = _v; }

} // end of namespace MiniScript
