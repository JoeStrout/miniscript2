// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AST.cs

#pragma once
#include "core_includes.h"
// AST.cs - Abstract Syntax Tree nodes for MiniScript
// These classes use the smart-pointer-wrapper pattern when transpiled to C++.


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
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




















// Base class for all AST nodes.
// When transpiled to C++, these become shared_ptr-wrapped classes.
struct ASTNode {
	protected: std::shared_ptr<ASTNodeStorage> storage;
  public:
	ASTNode(std::shared_ptr<ASTNodeStorage> stor) : storage(stor) {}
	ASTNode() : storage(nullptr) {}
	friend bool IsNull(ASTNode inst) { return inst.storage == nullptr; }
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(ASTNode node) {
		StorageType* stor = dynamic_cast<StorageType*>(node.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(node.storage);
	}

	public: String ToStr();
	public: ASTNode Simplify();
}; // end of struct ASTNode

template<typename WrapperType, typename StorageType> WrapperType As(ASTNode inst);

class ASTNodeStorage : public std::enable_shared_from_this<ASTNodeStorage> {
	public: virtual ~ASTNodeStorage() {}
	public: virtual String ToStr() = 0;
	public: virtual ASTNode Simplify() = 0;
	// Each node type should override this to provide a string representation

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)
}; // end of class ASTNodeStorage


// Number literal node (e.g., 42, 3.14)
class NumberNodeStorage : public ASTNodeStorage {
	public: Double Value;

	public: NumberNodeStorage(Double value);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class NumberNodeStorage


// String literal node (e.g., "hello")
class StringNodeStorage : public ASTNodeStorage {
	public: String Value;

	public: StringNodeStorage(String value);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class StringNodeStorage


// Identifier node (e.g., variable name like "x" or "foo")
class IdentifierNodeStorage : public ASTNodeStorage {
	public: String Name;

	public: IdentifierNodeStorage(String name);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class IdentifierNodeStorage


// Assignment node (e.g., x = 42, foo = bar + 1)
class AssignmentNodeStorage : public ASTNodeStorage {
	public: String Variable; // variable name being assigned to
	public: ASTNode Value; // expression being assigned

	public: AssignmentNodeStorage(String variable, ASTNode value);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class AssignmentNodeStorage


// Unary operator node (e.g., -x, not flag)
class UnaryOpNodeStorage : public ASTNodeStorage {
	public: String Op; // Op.MINUS, Op.NOT, etc.
	public: ASTNode Operand; // the expression being operated on

	public: UnaryOpNodeStorage(String op, ASTNode operand);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class UnaryOpNodeStorage


// Binary operator node (e.g., x + y, a * b)
class BinaryOpNodeStorage : public ASTNodeStorage {
	public: String Op; // Op.PLUS, etc.
	public: ASTNode Left; // left operand
	public: ASTNode Right; // right operand

	public: BinaryOpNodeStorage(String op, ASTNode left, ASTNode right);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class BinaryOpNodeStorage


// Function call node (e.g., sqrt(x), max(a, b))
class CallNodeStorage : public ASTNodeStorage {
	public: String Function; // function name
	public: List<ASTNode> Arguments; // list of argument expressions

	public: CallNodeStorage(String function, List<ASTNode> arguments);

	public: CallNodeStorage(String function);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class CallNodeStorage


// Grouping node (e.g., parenthesized expression like "(x + y)")
// Useful for preserving structure for pretty-printing or code generation.
class GroupNodeStorage : public ASTNodeStorage {
	public: ASTNode Expression;

	public: GroupNodeStorage(ASTNode expression);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class GroupNodeStorage


// List literal node (e.g., [1, 2, 3])
class ListNodeStorage : public ASTNodeStorage {
	public: List<ASTNode> Elements;

	public: ListNodeStorage(List<ASTNode> elements);

	public: ListNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class ListNodeStorage


// Map literal node (e.g., {"a": 1, "b": 2})
class MapNodeStorage : public ASTNodeStorage {
	public: List<ASTNode> Keys;
	public: List<ASTNode> Values;

	public: MapNodeStorage(List<ASTNode> keys, List<ASTNode> values);

	public: MapNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class MapNodeStorage


// Index access node (e.g., list[0], map["key"])
class IndexNodeStorage : public ASTNodeStorage {
	public: ASTNode Target; // the list/map/string being indexed
	public: ASTNode Index; // the index expression

	public: IndexNodeStorage(ASTNode target, ASTNode index);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class IndexNodeStorage


// Member access node (e.g., obj.field)
class MemberNodeStorage : public ASTNodeStorage {
	public: ASTNode Target; // the object being accessed
	public: String Member; // the member name

	public: MemberNodeStorage(ASTNode target, String member);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class MemberNodeStorage


// Method call node (e.g., obj.method(x, y))
// This is distinct from CallNode which handles simple function calls.
class MethodCallNodeStorage : public ASTNodeStorage {
	public: ASTNode Target; // the object whose method is being called
	public: String Method; // the method name
	public: List<ASTNode> Arguments; // list of argument expressions

	public: MethodCallNodeStorage(ASTNode target, String method, List<ASTNode> arguments);

	public: String ToStr();

	public: ASTNode Simplify();
}; // end of class MethodCallNodeStorage


// Number literal node (e.g., 42, 3.14)
struct NumberNode : public ASTNode {
	public: NumberNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: NumberNodeStorage* get() { return static_cast<NumberNodeStorage*>(storage.get()); }
	public: Double Value() { return get()->Value; }
	public: void set_Value(Double _v) { get()->Value = _v; }

	public: static NumberNode New(Double value) {
		return NumberNode(std::make_shared<NumberNodeStorage>(value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct NumberNode


// String literal node (e.g., "hello")
struct StringNode : public ASTNode {
	public: StringNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: StringNodeStorage* get() { return static_cast<StringNodeStorage*>(storage.get()); }
	public: String Value() { return get()->Value; }
	public: void set_Value(String _v) { get()->Value = _v; }

	public: static StringNode New(String value) {
		return StringNode(std::make_shared<StringNodeStorage>(value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct StringNode


// Identifier node (e.g., variable name like "x" or "foo")
struct IdentifierNode : public ASTNode {
	public: IdentifierNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: IdentifierNodeStorage* get() { return static_cast<IdentifierNodeStorage*>(storage.get()); }
	public: String Name() { return get()->Name; }
	public: void set_Name(String _v) { get()->Name = _v; }

	public: static IdentifierNode New(String name) {
		return IdentifierNode(std::make_shared<IdentifierNodeStorage>(name));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct IdentifierNode


// Assignment node (e.g., x = 42, foo = bar + 1)
struct AssignmentNode : public ASTNode {
	public: AssignmentNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: AssignmentNodeStorage* get() { return static_cast<AssignmentNodeStorage*>(storage.get()); }
	public: String Variable() { return get()->Variable; } // variable name being assigned to
	public: void set_Variable(String _v) { get()->Variable = _v; } // variable name being assigned to
	public: ASTNode Value() { return get()->Value; } // expression being assigned
	public: void set_Value(ASTNode _v) { get()->Value = _v; } // expression being assigned

	public: static AssignmentNode New(String variable, ASTNode value) {
		return AssignmentNode(std::make_shared<AssignmentNodeStorage>(variable, value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct AssignmentNode


// Unary operator node (e.g., -x, not flag)
struct UnaryOpNode : public ASTNode {
	public: UnaryOpNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: UnaryOpNodeStorage* get() { return static_cast<UnaryOpNodeStorage*>(storage.get()); }
	public: String Op() { return get()->Op; } // Op.MINUS, Op.NOT, etc.
	public: void set_Op(String _v) { get()->Op = _v; } // Op.MINUS, Op.NOT, etc.
	public: ASTNode Operand() { return get()->Operand; } // the expression being operated on
	public: void set_Operand(ASTNode _v) { get()->Operand = _v; } // the expression being operated on

	public: static UnaryOpNode New(String op, ASTNode operand) {
		return UnaryOpNode(std::make_shared<UnaryOpNodeStorage>(op, operand));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct UnaryOpNode


// Binary operator node (e.g., x + y, a * b)
struct BinaryOpNode : public ASTNode {
	public: BinaryOpNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: BinaryOpNodeStorage* get() { return static_cast<BinaryOpNodeStorage*>(storage.get()); }
	public: String Op() { return get()->Op; } // Op.PLUS, etc.
	public: void set_Op(String _v) { get()->Op = _v; } // Op.PLUS, etc.
	public: ASTNode Left() { return get()->Left; } // left operand
	public: void set_Left(ASTNode _v) { get()->Left = _v; } // left operand
	public: ASTNode Right() { return get()->Right; } // right operand
	public: void set_Right(ASTNode _v) { get()->Right = _v; } // right operand

	public: static BinaryOpNode New(String op, ASTNode left, ASTNode right) {
		return BinaryOpNode(std::make_shared<BinaryOpNodeStorage>(op, left, right));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct BinaryOpNode


// Function call node (e.g., sqrt(x), max(a, b))
struct CallNode : public ASTNode {
	public: CallNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: CallNodeStorage* get() { return static_cast<CallNodeStorage*>(storage.get()); }
	public: String Function() { return get()->Function; } // function name
	public: void set_Function(String _v) { get()->Function = _v; } // function name
	public: List<ASTNode> Arguments() { return get()->Arguments; } // list of argument expressions
	public: void set_Arguments(List<ASTNode> _v) { get()->Arguments = _v; } // list of argument expressions

	public: static CallNode New(String function, List<ASTNode> arguments) {
		return CallNode(std::make_shared<CallNodeStorage>(function, arguments));
	}

	public: static CallNode New(String function) {
		return CallNode(std::make_shared<CallNodeStorage>(function));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct CallNode


// Grouping node (e.g., parenthesized expression like "(x + y)")
// Useful for preserving structure for pretty-printing or code generation.
struct GroupNode : public ASTNode {
	public: GroupNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: GroupNodeStorage* get() { return static_cast<GroupNodeStorage*>(storage.get()); }
	public: ASTNode Expression() { return get()->Expression; }
	public: void set_Expression(ASTNode _v) { get()->Expression = _v; }

	public: static GroupNode New(ASTNode expression) {
		return GroupNode(std::make_shared<GroupNodeStorage>(expression));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct GroupNode


// List literal node (e.g., [1, 2, 3])
struct ListNode : public ASTNode {
	public: ListNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: ListNodeStorage* get() { return static_cast<ListNodeStorage*>(storage.get()); }
	public: List<ASTNode> Elements() { return get()->Elements; }
	public: void set_Elements(List<ASTNode> _v) { get()->Elements = _v; }

	public: static ListNode New(List<ASTNode> elements) {
		return ListNode(std::make_shared<ListNodeStorage>(elements));
	}

	public: static ListNode New() {
		return ListNode(std::make_shared<ListNodeStorage>());
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct ListNode


// Map literal node (e.g., {"a": 1, "b": 2})
struct MapNode : public ASTNode {
	public: MapNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: MapNodeStorage* get() { return static_cast<MapNodeStorage*>(storage.get()); }
	public: List<ASTNode> Keys() { return get()->Keys; }
	public: void set_Keys(List<ASTNode> _v) { get()->Keys = _v; }
	public: List<ASTNode> Values() { return get()->Values; }
	public: void set_Values(List<ASTNode> _v) { get()->Values = _v; }

	public: static MapNode New(List<ASTNode> keys, List<ASTNode> values) {
		return MapNode(std::make_shared<MapNodeStorage>(keys, values));
	}

	public: static MapNode New() {
		return MapNode(std::make_shared<MapNodeStorage>());
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct MapNode


// Index access node (e.g., list[0], map["key"])
struct IndexNode : public ASTNode {
	public: IndexNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: IndexNodeStorage* get() { return static_cast<IndexNodeStorage*>(storage.get()); }
	public: ASTNode Target() { return get()->Target; } // the list/map/string being indexed
	public: void set_Target(ASTNode _v) { get()->Target = _v; } // the list/map/string being indexed
	public: ASTNode Index() { return get()->Index; } // the index expression
	public: void set_Index(ASTNode _v) { get()->Index = _v; } // the index expression

	public: static IndexNode New(ASTNode target, ASTNode index) {
		return IndexNode(std::make_shared<IndexNodeStorage>(target, index));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct IndexNode


// Member access node (e.g., obj.field)
struct MemberNode : public ASTNode {
	public: MemberNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: MemberNodeStorage* get() { return static_cast<MemberNodeStorage*>(storage.get()); }
	public: ASTNode Target() { return get()->Target; } // the object being accessed
	public: void set_Target(ASTNode _v) { get()->Target = _v; } // the object being accessed
	public: String Member() { return get()->Member; } // the member name
	public: void set_Member(String _v) { get()->Member = _v; } // the member name

	public: static MemberNode New(ASTNode target, String member) {
		return MemberNode(std::make_shared<MemberNodeStorage>(target, member));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct MemberNode


// Method call node (e.g., obj.method(x, y))
// This is distinct from CallNode which handles simple function calls.
struct MethodCallNode : public ASTNode {
	public: MethodCallNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: MethodCallNodeStorage* get() { return static_cast<MethodCallNodeStorage*>(storage.get()); }
	public: ASTNode Target() { return get()->Target; } // the object whose method is being called
	public: void set_Target(ASTNode _v) { get()->Target = _v; } // the object whose method is being called
	public: String Method() { return get()->Method; } // the method name
	public: void set_Method(String _v) { get()->Method = _v; } // the method name
	public: List<ASTNode> Arguments() { return get()->Arguments; } // list of argument expressions
	public: void set_Arguments(List<ASTNode> _v) { get()->Arguments = _v; } // list of argument expressions

	public: static MethodCallNode New(ASTNode target, String method, List<ASTNode> arguments) {
		return MethodCallNode(std::make_shared<MethodCallNodeStorage>(target, method, arguments));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }
}; // end of struct MethodCallNode


// INLINE METHODS

inline String ASTNode::ToStr() { return storage->ToStr(); }
inline ASTNode ASTNode::Simplify() { return storage->Simplify(); }

} // end of namespace MiniScript
