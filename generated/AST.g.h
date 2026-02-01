// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AST.cs

#pragma once
#include "core_includes.h"
// AST.cs - Abstract Syntax Tree nodes for MiniScript
// These classes use the smart-pointer-wrapper pattern when transpiled to C++.


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





































// Operator constants (stored as strings to ease debugging)
class Op {
	public: static const String PLUS;
	public: static const String MINUS;
	public: static const String TIMES;
	public: static const String DIVIDE;
	public: static const String MOD;
	public: static const String POWER;
	public: static const String EQUALS;
	public: static const String NOT_EQUAL;
	public: static const String LESS_THAN;
	public: static const String GREATER_THAN;
	public: static const String LESS_EQUAL;
	public: static const String GREATER_EQUAL;
	public: static const String AND;
	public: static const String OR;
	public: static const String NOT;
}; // end of struct Op


// Visitor interface for AST traversal (e.g., code generation)
class IASTVisitor {
  public:
	virtual ~IASTVisitor() = default;
	virtual Int32 Visit(NumberNode node) = 0;
	virtual Int32 Visit(StringNode node) = 0;
	virtual Int32 Visit(IdentifierNode node) = 0;
	virtual Int32 Visit(AssignmentNode node) = 0;
	virtual Int32 Visit(UnaryOpNode node) = 0;
	virtual Int32 Visit(BinaryOpNode node) = 0;
	virtual Int32 Visit(CallNode node) = 0;
	virtual Int32 Visit(GroupNode node) = 0;
	virtual Int32 Visit(ListNode node) = 0;
	virtual Int32 Visit(MapNode node) = 0;
	virtual Int32 Visit(IndexNode node) = 0;
	virtual Int32 Visit(MemberNode node) = 0;
	virtual Int32 Visit(MethodCallNode node) = 0;
}; // end of interface IASTVisitor



















// Base class for all AST nodes.
// When transpiled to C++, these become shared_ptr-wrapped classes.
struct ASTNode {
	protected: std::shared_ptr<ASTNodeStorage> storage;
  public:
	ASTNode(std::shared_ptr<ASTNodeStorage> stor) : storage(stor) {}
	ASTNode() : storage(nullptr) {}
	friend bool IsNull(ASTNode inst) { return inst.storage == nullptr; }
	private: ASTNodeStorage* get() const;
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(ASTNode inst) {
		StorageType* stor = dynamic_cast<StorageType*>(inst.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(inst.storage);
	}

	public: String ToStr();
	public: ASTNode Simplify();
	public: Int32 Accept(IASTVisitor& visitor);
}; // end of struct ASTNode

template<typename WrapperType, typename StorageType> WrapperType As(ASTNode inst);

class ASTNodeStorage : public std::enable_shared_from_this<ASTNodeStorage> {
	public: virtual ~ASTNodeStorage() {}
	public: virtual String ToStr() = 0;
	public: virtual ASTNode Simplify() = 0;
	public: virtual Int32 Accept(IASTVisitor& visitor) = 0;
	// Each node type should override this to provide a string representation

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)

	// Visitor pattern: accept a visitor and return the result (e.g., register number)
}; // end of class ASTNodeStorage


// Number literal node (e.g., 42, 3.14)
class NumberNodeStorage : public ASTNodeStorage {
	friend struct NumberNode;
	public: Double Value;

	public: NumberNodeStorage(Double value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class NumberNodeStorage


// String literal node (e.g., "hello")
class StringNodeStorage : public ASTNodeStorage {
	friend struct StringNode;
	public: String Value;

	public: StringNodeStorage(String value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class StringNodeStorage


// Identifier node (e.g., variable name like "x" or "foo")
class IdentifierNodeStorage : public ASTNodeStorage {
	friend struct IdentifierNode;
	public: String Name;

	public: IdentifierNodeStorage(String name);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class IdentifierNodeStorage


// Assignment node (e.g., x = 42, foo = bar + 1)
class AssignmentNodeStorage : public ASTNodeStorage {
	friend struct AssignmentNode;
	public: String Variable; // variable name being assigned to
	public: ASTNode Value; // expression being assigned

	public: AssignmentNodeStorage(String variable, ASTNode value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class AssignmentNodeStorage


// Unary operator node (e.g., -x, not flag)
class UnaryOpNodeStorage : public ASTNodeStorage {
	friend struct UnaryOpNode;
	public: String Op; // Op.MINUS, Op.NOT, etc.
	public: ASTNode Operand; // the expression being operated on

	public: UnaryOpNodeStorage(String op, ASTNode operand);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class UnaryOpNodeStorage


// Binary operator node (e.g., x + y, a * b)
class BinaryOpNodeStorage : public ASTNodeStorage {
	friend struct BinaryOpNode;
	public: String Op; // Op.PLUS, etc.
	public: ASTNode Left; // left operand
	public: ASTNode Right; // right operand

	public: BinaryOpNodeStorage(String op, ASTNode left, ASTNode right);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class BinaryOpNodeStorage


// Function call node (e.g., sqrt(x), max(a, b))
class CallNodeStorage : public ASTNodeStorage {
	friend struct CallNode;
	public: String Function; // function name
	public: List<ASTNode> Arguments; // list of argument expressions

	public: CallNodeStorage(String function, List<ASTNode> arguments);

	public: CallNodeStorage(String function);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class CallNodeStorage


// Grouping node (e.g., parenthesized expression like "(x + y)")
// Useful for preserving structure for pretty-printing or code generation.
class GroupNodeStorage : public ASTNodeStorage {
	friend struct GroupNode;
	public: ASTNode Expression;

	public: GroupNodeStorage(ASTNode expression);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class GroupNodeStorage


// List literal node (e.g., [1, 2, 3])
class ListNodeStorage : public ASTNodeStorage {
	friend struct ListNode;
	public: List<ASTNode> Elements;

	public: ListNodeStorage(List<ASTNode> elements);

	public: ListNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class ListNodeStorage


// Map literal node (e.g., {"a": 1, "b": 2})
class MapNodeStorage : public ASTNodeStorage {
	friend struct MapNode;
	public: List<ASTNode> Keys;
	public: List<ASTNode> Values;

	public: MapNodeStorage(List<ASTNode> keys, List<ASTNode> values);

	public: MapNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class MapNodeStorage


// Index access node (e.g., list[0], map["key"])
class IndexNodeStorage : public ASTNodeStorage {
	friend struct IndexNode;
	public: ASTNode Target; // the list/map/string being indexed
	public: ASTNode Index; // the index expression

	public: IndexNodeStorage(ASTNode target, ASTNode index);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class IndexNodeStorage


// Member access node (e.g., obj.field)
class MemberNodeStorage : public ASTNodeStorage {
	friend struct MemberNode;
	public: ASTNode Target; // the object being accessed
	public: String Member; // the member name

	public: MemberNodeStorage(ASTNode target, String member);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class MemberNodeStorage


// Method call node (e.g., obj.method(x, y))
// This is distinct from CallNode which handles simple function calls.
class MethodCallNodeStorage : public ASTNodeStorage {
	friend struct MethodCallNode;
	public: ASTNode Target; // the object whose method is being called
	public: String Method; // the method name
	public: List<ASTNode> Arguments; // list of argument expressions

	public: MethodCallNodeStorage(ASTNode target, String method, List<ASTNode> arguments);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class MethodCallNodeStorage


// Number literal node (e.g., 42, 3.14)
struct NumberNode : public ASTNode {
	public: NumberNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	NumberNode(std::nullptr_t) : ASTNode(nullptr) {}
	NumberNode() : ASTNode(nullptr) {}
	private: NumberNodeStorage* get();
	public: Double Value();
	public: void set_Value(Double _v);

	public: static NumberNode New(Double value) {
		return NumberNode(std::make_shared<NumberNodeStorage>(value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct NumberNode


// String literal node (e.g., "hello")
struct StringNode : public ASTNode {
	public: StringNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	StringNode(std::nullptr_t) : ASTNode(nullptr) {}
	StringNode() : ASTNode(nullptr) {}
	private: StringNodeStorage* get();
	public: String Value();
	public: void set_Value(String _v);

	public: static StringNode New(String value) {
		return StringNode(std::make_shared<StringNodeStorage>(value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct StringNode


// Identifier node (e.g., variable name like "x" or "foo")
struct IdentifierNode : public ASTNode {
	public: IdentifierNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	IdentifierNode(std::nullptr_t) : ASTNode(nullptr) {}
	IdentifierNode() : ASTNode(nullptr) {}
	private: IdentifierNodeStorage* get();
	public: String Name();
	public: void set_Name(String _v);

	public: static IdentifierNode New(String name) {
		return IdentifierNode(std::make_shared<IdentifierNodeStorage>(name));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct IdentifierNode


// Assignment node (e.g., x = 42, foo = bar + 1)
struct AssignmentNode : public ASTNode {
	public: AssignmentNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	AssignmentNode(std::nullptr_t) : ASTNode(nullptr) {}
	AssignmentNode() : ASTNode(nullptr) {}
	private: AssignmentNodeStorage* get();
	public: String Variable(); // variable name being assigned to
	public: void set_Variable(String _v); // variable name being assigned to
	public: ASTNode Value(); // expression being assigned
	public: void set_Value(ASTNode _v); // expression being assigned

	public: static AssignmentNode New(String variable, ASTNode value) {
		return AssignmentNode(std::make_shared<AssignmentNodeStorage>(variable, value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct AssignmentNode


// Unary operator node (e.g., -x, not flag)
struct UnaryOpNode : public ASTNode {
	public: UnaryOpNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	UnaryOpNode(std::nullptr_t) : ASTNode(nullptr) {}
	UnaryOpNode() : ASTNode(nullptr) {}
	private: UnaryOpNodeStorage* get();
	public: String Op(); // Op.MINUS, Op.NOT, etc.
	public: void set_Op(String _v); // Op.MINUS, Op.NOT, etc.
	public: ASTNode Operand(); // the expression being operated on
	public: void set_Operand(ASTNode _v); // the expression being operated on

	public: static UnaryOpNode New(String op, ASTNode operand) {
		return UnaryOpNode(std::make_shared<UnaryOpNodeStorage>(op, operand));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct UnaryOpNode


// Binary operator node (e.g., x + y, a * b)
struct BinaryOpNode : public ASTNode {
	public: BinaryOpNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	BinaryOpNode(std::nullptr_t) : ASTNode(nullptr) {}
	BinaryOpNode() : ASTNode(nullptr) {}
	private: BinaryOpNodeStorage* get();
	public: String Op(); // Op.PLUS, etc.
	public: void set_Op(String _v); // Op.PLUS, etc.
	public: ASTNode Left(); // left operand
	public: void set_Left(ASTNode _v); // left operand
	public: ASTNode Right(); // right operand
	public: void set_Right(ASTNode _v); // right operand

	public: static BinaryOpNode New(String op, ASTNode left, ASTNode right) {
		return BinaryOpNode(std::make_shared<BinaryOpNodeStorage>(op, left, right));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct BinaryOpNode


// Function call node (e.g., sqrt(x), max(a, b))
struct CallNode : public ASTNode {
	public: CallNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	CallNode(std::nullptr_t) : ASTNode(nullptr) {}
	CallNode() : ASTNode(nullptr) {}
	private: CallNodeStorage* get();
	public: String Function(); // function name
	public: void set_Function(String _v); // function name
	public: List<ASTNode> Arguments(); // list of argument expressions
	public: void set_Arguments(List<ASTNode> _v); // list of argument expressions

	public: static CallNode New(String function, List<ASTNode> arguments) {
		return CallNode(std::make_shared<CallNodeStorage>(function, arguments));
	}

	public: static CallNode New(String function) {
		return CallNode(std::make_shared<CallNodeStorage>(function));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct CallNode


// Grouping node (e.g., parenthesized expression like "(x + y)")
// Useful for preserving structure for pretty-printing or code generation.
struct GroupNode : public ASTNode {
	public: GroupNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	GroupNode(std::nullptr_t) : ASTNode(nullptr) {}
	GroupNode() : ASTNode(nullptr) {}
	private: GroupNodeStorage* get();
	public: ASTNode Expression();
	public: void set_Expression(ASTNode _v);

	public: static GroupNode New(ASTNode expression) {
		return GroupNode(std::make_shared<GroupNodeStorage>(expression));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct GroupNode


// List literal node (e.g., [1, 2, 3])
struct ListNode : public ASTNode {
	public: ListNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	ListNode(std::nullptr_t) : ASTNode(nullptr) {}
	ListNode() : ASTNode(nullptr) {}
	private: ListNodeStorage* get();
	public: List<ASTNode> Elements();
	public: void set_Elements(List<ASTNode> _v);

	public: static ListNode New(List<ASTNode> elements) {
		return ListNode(std::make_shared<ListNodeStorage>(elements));
	}

	public: static ListNode New() {
		return ListNode(std::make_shared<ListNodeStorage>());
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct ListNode


// Map literal node (e.g., {"a": 1, "b": 2})
struct MapNode : public ASTNode {
	public: MapNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	MapNode(std::nullptr_t) : ASTNode(nullptr) {}
	MapNode() : ASTNode(nullptr) {}
	private: MapNodeStorage* get();
	public: List<ASTNode> Keys();
	public: void set_Keys(List<ASTNode> _v);
	public: List<ASTNode> Values();
	public: void set_Values(List<ASTNode> _v);

	public: static MapNode New(List<ASTNode> keys, List<ASTNode> values) {
		return MapNode(std::make_shared<MapNodeStorage>(keys, values));
	}

	public: static MapNode New() {
		return MapNode(std::make_shared<MapNodeStorage>());
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct MapNode


// Index access node (e.g., list[0], map["key"])
struct IndexNode : public ASTNode {
	public: IndexNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	IndexNode(std::nullptr_t) : ASTNode(nullptr) {}
	IndexNode() : ASTNode(nullptr) {}
	private: IndexNodeStorage* get();
	public: ASTNode Target(); // the list/map/string being indexed
	public: void set_Target(ASTNode _v); // the list/map/string being indexed
	public: ASTNode Index(); // the index expression
	public: void set_Index(ASTNode _v); // the index expression

	public: static IndexNode New(ASTNode target, ASTNode index) {
		return IndexNode(std::make_shared<IndexNodeStorage>(target, index));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct IndexNode


// Member access node (e.g., obj.field)
struct MemberNode : public ASTNode {
	public: MemberNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	MemberNode(std::nullptr_t) : ASTNode(nullptr) {}
	MemberNode() : ASTNode(nullptr) {}
	private: MemberNodeStorage* get();
	public: ASTNode Target(); // the object being accessed
	public: void set_Target(ASTNode _v); // the object being accessed
	public: String Member(); // the member name
	public: void set_Member(String _v); // the member name

	public: static MemberNode New(ASTNode target, String member) {
		return MemberNode(std::make_shared<MemberNodeStorage>(target, member));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct MemberNode


// Method call node (e.g., obj.method(x, y))
// This is distinct from CallNode which handles simple function calls.
struct MethodCallNode : public ASTNode {
	public: MethodCallNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	MethodCallNode(std::nullptr_t) : ASTNode(nullptr) {}
	MethodCallNode() : ASTNode(nullptr) {}
	private: MethodCallNodeStorage* get();
	public: ASTNode Target(); // the object whose method is being called
	public: void set_Target(ASTNode _v); // the object whose method is being called
	public: String Method(); // the method name
	public: void set_Method(String _v); // the method name
	public: List<ASTNode> Arguments(); // list of argument expressions
	public: void set_Arguments(List<ASTNode> _v); // list of argument expressions

	public: static MethodCallNode New(ASTNode target, String method, List<ASTNode> arguments) {
		return MethodCallNode(std::make_shared<MethodCallNodeStorage>(target, method, arguments));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct MethodCallNode


// INLINE METHODS

inline ASTNodeStorage* ASTNode::get() const { return static_cast<ASTNodeStorage*>(storage.get()); }
inline String ASTNode::ToStr() { return get()->ToStr(); }
inline ASTNode ASTNode::Simplify() { return get()->Simplify(); }
inline Int32 ASTNode::Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }

inline NumberNodeStorage* NumberNode::get() { return static_cast<NumberNodeStorage*>(storage.get()); }
inline Double NumberNode::Value() { return get()->Value; }
inline void NumberNode::set_Value(Double _v) { get()->Value = _v; }

inline StringNodeStorage* StringNode::get() { return static_cast<StringNodeStorage*>(storage.get()); }
inline String StringNode::Value() { return get()->Value; }
inline void StringNode::set_Value(String _v) { get()->Value = _v; }

inline IdentifierNodeStorage* IdentifierNode::get() { return static_cast<IdentifierNodeStorage*>(storage.get()); }
inline String IdentifierNode::Name() { return get()->Name; }
inline void IdentifierNode::set_Name(String _v) { get()->Name = _v; }

inline AssignmentNodeStorage* AssignmentNode::get() { return static_cast<AssignmentNodeStorage*>(storage.get()); }
inline String AssignmentNode::Variable() { return get()->Variable; } // variable name being assigned to
inline void AssignmentNode::set_Variable(String _v) { get()->Variable = _v; } // variable name being assigned to
inline ASTNode AssignmentNode::Value() { return get()->Value; } // expression being assigned
inline void AssignmentNode::set_Value(ASTNode _v) { get()->Value = _v; } // expression being assigned

inline UnaryOpNodeStorage* UnaryOpNode::get() { return static_cast<UnaryOpNodeStorage*>(storage.get()); }
inline String UnaryOpNode::Op() { return get()->Op; } // Op.MINUS, Op.NOT, etc.
inline void UnaryOpNode::set_Op(String _v) { get()->Op = _v; } // Op.MINUS, Op.NOT, etc.
inline ASTNode UnaryOpNode::Operand() { return get()->Operand; } // the expression being operated on
inline void UnaryOpNode::set_Operand(ASTNode _v) { get()->Operand = _v; } // the expression being operated on

inline BinaryOpNodeStorage* BinaryOpNode::get() { return static_cast<BinaryOpNodeStorage*>(storage.get()); }
inline String BinaryOpNode::Op() { return get()->Op; } // Op.PLUS, etc.
inline void BinaryOpNode::set_Op(String _v) { get()->Op = _v; } // Op.PLUS, etc.
inline ASTNode BinaryOpNode::Left() { return get()->Left; } // left operand
inline void BinaryOpNode::set_Left(ASTNode _v) { get()->Left = _v; } // left operand
inline ASTNode BinaryOpNode::Right() { return get()->Right; } // right operand
inline void BinaryOpNode::set_Right(ASTNode _v) { get()->Right = _v; } // right operand

inline CallNodeStorage* CallNode::get() { return static_cast<CallNodeStorage*>(storage.get()); }
inline String CallNode::Function() { return get()->Function; } // function name
inline void CallNode::set_Function(String _v) { get()->Function = _v; } // function name
inline List<ASTNode> CallNode::Arguments() { return get()->Arguments; } // list of argument expressions
inline void CallNode::set_Arguments(List<ASTNode> _v) { get()->Arguments = _v; } // list of argument expressions

inline GroupNodeStorage* GroupNode::get() { return static_cast<GroupNodeStorage*>(storage.get()); }
inline ASTNode GroupNode::Expression() { return get()->Expression; }
inline void GroupNode::set_Expression(ASTNode _v) { get()->Expression = _v; }

inline ListNodeStorage* ListNode::get() { return static_cast<ListNodeStorage*>(storage.get()); }
inline List<ASTNode> ListNode::Elements() { return get()->Elements; }
inline void ListNode::set_Elements(List<ASTNode> _v) { get()->Elements = _v; }

inline MapNodeStorage* MapNode::get() { return static_cast<MapNodeStorage*>(storage.get()); }
inline List<ASTNode> MapNode::Keys() { return get()->Keys; }
inline void MapNode::set_Keys(List<ASTNode> _v) { get()->Keys = _v; }
inline List<ASTNode> MapNode::Values() { return get()->Values; }
inline void MapNode::set_Values(List<ASTNode> _v) { get()->Values = _v; }

inline IndexNodeStorage* IndexNode::get() { return static_cast<IndexNodeStorage*>(storage.get()); }
inline ASTNode IndexNode::Target() { return get()->Target; } // the list/map/string being indexed
inline void IndexNode::set_Target(ASTNode _v) { get()->Target = _v; } // the list/map/string being indexed
inline ASTNode IndexNode::Index() { return get()->Index; } // the index expression
inline void IndexNode::set_Index(ASTNode _v) { get()->Index = _v; } // the index expression

inline MemberNodeStorage* MemberNode::get() { return static_cast<MemberNodeStorage*>(storage.get()); }
inline ASTNode MemberNode::Target() { return get()->Target; } // the object being accessed
inline void MemberNode::set_Target(ASTNode _v) { get()->Target = _v; } // the object being accessed
inline String MemberNode::Member() { return get()->Member; } // the member name
inline void MemberNode::set_Member(String _v) { get()->Member = _v; } // the member name

inline MethodCallNodeStorage* MethodCallNode::get() { return static_cast<MethodCallNodeStorage*>(storage.get()); }
inline ASTNode MethodCallNode::Target() { return get()->Target; } // the object whose method is being called
inline void MethodCallNode::set_Target(ASTNode _v) { get()->Target = _v; } // the object whose method is being called
inline String MethodCallNode::Method() { return get()->Method; } // the method name
inline void MethodCallNode::set_Method(String _v) { get()->Method = _v; } // the method name
inline List<ASTNode> MethodCallNode::Arguments() { return get()->Arguments; } // list of argument expressions
inline void MethodCallNode::set_Arguments(List<ASTNode> _v) { get()->Arguments = _v; } // list of argument expressions

} // end of namespace MiniScript
