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






































// Operator constants (stored as strings to ease debugging)
class Op {
	public: static const String PLUS;
	public: static const String MINUS;
	public: static const String TIMES;
	public: static const String DIVIDE;
	public: static const String MOD;
	public: static const String POWER;
	public: static const String ADDRESS_OF;
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
	virtual Int32 Visit(WhileNode node) = 0;
	virtual Int32 Visit(IfNode node) = 0;
	virtual Int32 Visit(ForNode node) = 0;
	virtual Int32 Visit(BreakNode node) = 0;
	virtual Int32 Visit(ContinueNode node) = 0;
	virtual Int32 Visit(FunctionNode node) = 0;
	virtual Int32 Visit(ReturnNode node) = 0;
}; // end of interface IASTVisitor


























// Base class for all AST nodes.
// When transpiled to C++, these become shared_ptr-wrapped classes.
struct ASTNode {
	friend class ASTNodeStorage;
	protected: std::shared_ptr<ASTNodeStorage> storage;
  public:
	ASTNode(std::shared_ptr<ASTNodeStorage> stor) : storage(stor) {}
	ASTNode() : storage(nullptr) {}
	ASTNode(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const ASTNode& inst) { return inst.storage == nullptr; }
	private: ASTNodeStorage* get() const;
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(ASTNode inst) {
		auto stor = std::dynamic_pointer_cast<StorageType>(inst.storage);
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(stor); 
	}

	public: String ToStr();
	public: ASTNode Simplify();
	public: Int32 Accept(IASTVisitor& visitor);
}; // end of struct ASTNode

template<typename WrapperType, typename StorageType> WrapperType As(ASTNode inst);

class ASTNodeStorage : public std::enable_shared_from_this<ASTNodeStorage> {
	friend struct ASTNode;
	public: virtual ~ASTNodeStorage() {}
	public: virtual String ToStr() = 0;
	public: virtual ASTNode Simplify() = 0;
	public: virtual Int32 Accept(IASTVisitor& visitor) = 0;
	// Each node type should override this to provide a string representation

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)

	// Visitor pattern: accept a visitor and return the result (e.g., register number)
}; // end of class ASTNodeStorage

class NumberNodeStorage : public ASTNodeStorage {
	friend struct NumberNode;
	public: Double Value;

	public: NumberNodeStorage(Double value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class NumberNodeStorage

class StringNodeStorage : public ASTNodeStorage {
	friend struct StringNode;
	public: String Value;

	public: StringNodeStorage(String value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class StringNodeStorage

class IdentifierNodeStorage : public ASTNodeStorage {
	friend struct IdentifierNode;
	public: String Name;

	public: IdentifierNodeStorage(String name);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class IdentifierNodeStorage

class AssignmentNodeStorage : public ASTNodeStorage {
	friend struct AssignmentNode;
	public: String Variable; // variable name being assigned to
	public: ASTNode Value; // expression being assigned

	public: AssignmentNodeStorage(String variable, ASTNode value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class AssignmentNodeStorage

class UnaryOpNodeStorage : public ASTNodeStorage {
	friend struct UnaryOpNode;
	public: String Op; // Op.MINUS, Op.NOT, etc.
	public: ASTNode Operand; // the expression being operated on

	public: UnaryOpNodeStorage(String op, ASTNode operand);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class UnaryOpNodeStorage

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

class GroupNodeStorage : public ASTNodeStorage {
	friend struct GroupNode;
	public: ASTNode Expression;

	public: GroupNodeStorage(ASTNode expression);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class GroupNodeStorage

class ListNodeStorage : public ASTNodeStorage {
	friend struct ListNode;
	public: List<ASTNode> Elements;

	public: ListNodeStorage(List<ASTNode> elements);

	public: ListNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class ListNodeStorage

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

class IndexNodeStorage : public ASTNodeStorage {
	friend struct IndexNode;
	public: ASTNode Target; // the list/map/string being indexed
	public: ASTNode Index; // the index expression

	public: IndexNodeStorage(ASTNode target, ASTNode index);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class IndexNodeStorage

class MemberNodeStorage : public ASTNodeStorage {
	friend struct MemberNode;
	public: ASTNode Target; // the object being accessed
	public: String Member; // the member name

	public: MemberNodeStorage(ASTNode target, String member);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class MemberNodeStorage

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

class WhileNodeStorage : public ASTNodeStorage {
	friend struct WhileNode;
	public: ASTNode Condition; // the loop condition
	public: List<ASTNode> Body; // statements in the loop body

	public: WhileNodeStorage(ASTNode condition, List<ASTNode> body);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class WhileNodeStorage

class IfNodeStorage : public ASTNodeStorage {
	friend struct IfNode;
	public: ASTNode Condition; // the test expression
	public: List<ASTNode> ThenBody; // statements if condition is true
	public: List<ASTNode> ElseBody; // statements if condition is false (may contain IfNode for else-if)

	public: IfNodeStorage(ASTNode condition, List<ASTNode> thenBody, List<ASTNode> elseBody);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class IfNodeStorage

class ForNodeStorage : public ASTNodeStorage {
	friend struct ForNode;
	public: String Variable; // the loop variable name
	public: ASTNode Iterable; // the expression to iterate over
	public: List<ASTNode> Body; // statements in the loop body

	public: ForNodeStorage(String variable, ASTNode iterable, List<ASTNode> body);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class ForNodeStorage

class BreakNodeStorage : public ASTNodeStorage {
	friend struct BreakNode;
	public: BreakNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class BreakNodeStorage

class ContinueNodeStorage : public ASTNodeStorage {
	friend struct ContinueNode;
	public: ContinueNodeStorage();

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class ContinueNodeStorage

class FunctionNodeStorage : public ASTNodeStorage {
	friend struct FunctionNode;
	public: List<String> ParamNames; // parameter names
	public: List<ASTNode> Body; // statements in the function body

	public: FunctionNodeStorage(List<String> paramNames, List<ASTNode> body);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class FunctionNodeStorage

class ReturnNodeStorage : public ASTNodeStorage {
	friend struct ReturnNode;
	public: ASTNode Value; // expression to return (null for bare return)

	public: ReturnNodeStorage(ASTNode value);

	public: String ToStr();

	public: ASTNode Simplify();

	public: Int32 Accept(IASTVisitor& visitor);
}; // end of class ReturnNodeStorage


// Number literal node (e.g., 42, 3.14)
struct NumberNode : public ASTNode {
	friend class NumberNodeStorage;
	NumberNode(std::shared_ptr<NumberNodeStorage> stor);
	NumberNode() : ASTNode() {}
	NumberNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: NumberNodeStorage* get() const;

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
	friend class StringNodeStorage;
	StringNode(std::shared_ptr<StringNodeStorage> stor);
	StringNode() : ASTNode() {}
	StringNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: StringNodeStorage* get() const;

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
	friend class IdentifierNodeStorage;
	IdentifierNode(std::shared_ptr<IdentifierNodeStorage> stor);
	IdentifierNode() : ASTNode() {}
	IdentifierNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: IdentifierNodeStorage* get() const;

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
	friend class AssignmentNodeStorage;
	AssignmentNode(std::shared_ptr<AssignmentNodeStorage> stor);
	AssignmentNode() : ASTNode() {}
	AssignmentNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: AssignmentNodeStorage* get() const;

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
	friend class UnaryOpNodeStorage;
	UnaryOpNode(std::shared_ptr<UnaryOpNodeStorage> stor);
	UnaryOpNode() : ASTNode() {}
	UnaryOpNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: UnaryOpNodeStorage* get() const;

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
	friend class BinaryOpNodeStorage;
	BinaryOpNode(std::shared_ptr<BinaryOpNodeStorage> stor);
	BinaryOpNode() : ASTNode() {}
	BinaryOpNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: BinaryOpNodeStorage* get() const;

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
	friend class CallNodeStorage;
	CallNode(std::shared_ptr<CallNodeStorage> stor);
	CallNode() : ASTNode() {}
	CallNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: CallNodeStorage* get() const;

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
	friend class GroupNodeStorage;
	GroupNode(std::shared_ptr<GroupNodeStorage> stor);
	GroupNode() : ASTNode() {}
	GroupNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: GroupNodeStorage* get() const;

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
	friend class ListNodeStorage;
	ListNode(std::shared_ptr<ListNodeStorage> stor);
	ListNode() : ASTNode() {}
	ListNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: ListNodeStorage* get() const;

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
	friend class MapNodeStorage;
	MapNode(std::shared_ptr<MapNodeStorage> stor);
	MapNode() : ASTNode() {}
	MapNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: MapNodeStorage* get() const;

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
	friend class IndexNodeStorage;
	IndexNode(std::shared_ptr<IndexNodeStorage> stor);
	IndexNode() : ASTNode() {}
	IndexNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: IndexNodeStorage* get() const;

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
	friend class MemberNodeStorage;
	MemberNode(std::shared_ptr<MemberNodeStorage> stor);
	MemberNode() : ASTNode() {}
	MemberNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: MemberNodeStorage* get() const;

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
	friend class MethodCallNodeStorage;
	MethodCallNode(std::shared_ptr<MethodCallNodeStorage> stor);
	MethodCallNode() : ASTNode() {}
	MethodCallNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: MethodCallNodeStorage* get() const;

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


// While loop node (e.g., while x < 10 ... end while)
struct WhileNode : public ASTNode {
	friend class WhileNodeStorage;
	WhileNode(std::shared_ptr<WhileNodeStorage> stor);
	WhileNode() : ASTNode() {}
	WhileNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: WhileNodeStorage* get() const;

	public: ASTNode Condition(); // the loop condition
	public: void set_Condition(ASTNode _v); // the loop condition
	public: List<ASTNode> Body(); // statements in the loop body
	public: void set_Body(List<ASTNode> _v); // statements in the loop body

	public: static WhileNode New(ASTNode condition, List<ASTNode> body) {
		return WhileNode(std::make_shared<WhileNodeStorage>(condition, body));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct WhileNode


// If statement node (e.g., if x > 0 then ... else ... end if)
// Handles both block and single-line forms; else-if chains are represented
// by an IfNode in the ElseBody.
struct IfNode : public ASTNode {
	friend class IfNodeStorage;
	IfNode(std::shared_ptr<IfNodeStorage> stor);
	IfNode() : ASTNode() {}
	IfNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: IfNodeStorage* get() const;

	public: ASTNode Condition(); // the test expression
	public: void set_Condition(ASTNode _v); // the test expression
	public: List<ASTNode> ThenBody(); // statements if condition is true
	public: void set_ThenBody(List<ASTNode> _v); // statements if condition is true
	public: List<ASTNode> ElseBody(); // statements if condition is false (may contain IfNode for else-if)
	public: void set_ElseBody(List<ASTNode> _v); // statements if condition is false (may contain IfNode for else-if)

	public: static IfNode New(ASTNode condition, List<ASTNode> thenBody, List<ASTNode> elseBody) {
		return IfNode(std::make_shared<IfNodeStorage>(condition, thenBody, elseBody));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct IfNode


// For loop node (e.g., for i in [1,2,3] ... end for)
struct ForNode : public ASTNode {
	friend class ForNodeStorage;
	ForNode(std::shared_ptr<ForNodeStorage> stor);
	ForNode() : ASTNode() {}
	ForNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: ForNodeStorage* get() const;

	public: String Variable(); // the loop variable name
	public: void set_Variable(String _v); // the loop variable name
	public: ASTNode Iterable(); // the expression to iterate over
	public: void set_Iterable(ASTNode _v); // the expression to iterate over
	public: List<ASTNode> Body(); // statements in the loop body
	public: void set_Body(List<ASTNode> _v); // statements in the loop body

	public: static ForNode New(String variable, ASTNode iterable, List<ASTNode> body) {
		return ForNode(std::make_shared<ForNodeStorage>(variable, iterable, body));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct ForNode


// Break statement node - exits the innermost loop
struct BreakNode : public ASTNode {
	friend class BreakNodeStorage;
	BreakNode(std::shared_ptr<BreakNodeStorage> stor);
	BreakNode() : ASTNode() {}
	BreakNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: BreakNodeStorage* get() const;

	public: static BreakNode New() {
		return BreakNode(std::make_shared<BreakNodeStorage>());
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct BreakNode


// Continue statement node - skips to next iteration of innermost loop
struct ContinueNode : public ASTNode {
	friend class ContinueNodeStorage;
	ContinueNode(std::shared_ptr<ContinueNodeStorage> stor);
	ContinueNode() : ASTNode() {}
	ContinueNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: ContinueNodeStorage* get() const;

	public: static ContinueNode New() {
		return ContinueNode(std::make_shared<ContinueNodeStorage>());
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct ContinueNode


// Function definition expression (e.g., function(x, y) ... end function)
struct FunctionNode : public ASTNode {
	friend class FunctionNodeStorage;
	FunctionNode(std::shared_ptr<FunctionNodeStorage> stor);
	FunctionNode() : ASTNode() {}
	FunctionNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: FunctionNodeStorage* get() const;

	public: List<String> ParamNames(); // parameter names
	public: void set_ParamNames(List<String> _v); // parameter names
	public: List<ASTNode> Body(); // statements in the function body
	public: void set_Body(List<ASTNode> _v); // statements in the function body

	public: static FunctionNode New(List<String> paramNames, List<ASTNode> body) {
		return FunctionNode(std::make_shared<FunctionNodeStorage>(paramNames, body));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct FunctionNode


// Return statement node (e.g., return x + 1)
struct ReturnNode : public ASTNode {
	friend class ReturnNodeStorage;
	ReturnNode(std::shared_ptr<ReturnNodeStorage> stor);
	ReturnNode() : ASTNode() {}
	ReturnNode(std::nullptr_t) : ASTNode(nullptr) {}
	private: ReturnNodeStorage* get() const;

	public: ASTNode Value(); // expression to return (null for bare return)
	public: void set_Value(ASTNode _v); // expression to return (null for bare return)

	public: static ReturnNode New(ASTNode value) {
		return ReturnNode(std::make_shared<ReturnNodeStorage>(value));
	}

	public: String ToStr() { return get()->ToStr(); }

	public: ASTNode Simplify() { return get()->Simplify(); }

	public: Int32 Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }
}; // end of struct ReturnNode


// INLINE METHODS

inline ASTNodeStorage* ASTNode::get() const { return static_cast<ASTNodeStorage*>(storage.get()); }
inline String ASTNode::ToStr() { return get()->ToStr(); }
inline ASTNode ASTNode::Simplify() { return get()->Simplify(); }
inline Int32 ASTNode::Accept(IASTVisitor& visitor) { return get()->Accept(visitor); }

inline NumberNode::NumberNode(std::shared_ptr<NumberNodeStorage> stor) : ASTNode(stor) {}
inline NumberNodeStorage* NumberNode::get() const { return static_cast<NumberNodeStorage*>(storage.get()); }
inline Double NumberNode::Value() { return get()->Value; }
inline void NumberNode::set_Value(Double _v) { get()->Value = _v; }

inline StringNode::StringNode(std::shared_ptr<StringNodeStorage> stor) : ASTNode(stor) {}
inline StringNodeStorage* StringNode::get() const { return static_cast<StringNodeStorage*>(storage.get()); }
inline String StringNode::Value() { return get()->Value; }
inline void StringNode::set_Value(String _v) { get()->Value = _v; }

inline IdentifierNode::IdentifierNode(std::shared_ptr<IdentifierNodeStorage> stor) : ASTNode(stor) {}
inline IdentifierNodeStorage* IdentifierNode::get() const { return static_cast<IdentifierNodeStorage*>(storage.get()); }
inline String IdentifierNode::Name() { return get()->Name; }
inline void IdentifierNode::set_Name(String _v) { get()->Name = _v; }

inline AssignmentNode::AssignmentNode(std::shared_ptr<AssignmentNodeStorage> stor) : ASTNode(stor) {}
inline AssignmentNodeStorage* AssignmentNode::get() const { return static_cast<AssignmentNodeStorage*>(storage.get()); }
inline String AssignmentNode::Variable() { return get()->Variable; } // variable name being assigned to
inline void AssignmentNode::set_Variable(String _v) { get()->Variable = _v; } // variable name being assigned to
inline ASTNode AssignmentNode::Value() { return get()->Value; } // expression being assigned
inline void AssignmentNode::set_Value(ASTNode _v) { get()->Value = _v; } // expression being assigned

inline UnaryOpNode::UnaryOpNode(std::shared_ptr<UnaryOpNodeStorage> stor) : ASTNode(stor) {}
inline UnaryOpNodeStorage* UnaryOpNode::get() const { return static_cast<UnaryOpNodeStorage*>(storage.get()); }
inline String UnaryOpNode::Op() { return get()->Op; } // Op.MINUS, Op.NOT, etc.
inline void UnaryOpNode::set_Op(String _v) { get()->Op = _v; } // Op.MINUS, Op.NOT, etc.
inline ASTNode UnaryOpNode::Operand() { return get()->Operand; } // the expression being operated on
inline void UnaryOpNode::set_Operand(ASTNode _v) { get()->Operand = _v; } // the expression being operated on

inline BinaryOpNode::BinaryOpNode(std::shared_ptr<BinaryOpNodeStorage> stor) : ASTNode(stor) {}
inline BinaryOpNodeStorage* BinaryOpNode::get() const { return static_cast<BinaryOpNodeStorage*>(storage.get()); }
inline String BinaryOpNode::Op() { return get()->Op; } // Op.PLUS, etc.
inline void BinaryOpNode::set_Op(String _v) { get()->Op = _v; } // Op.PLUS, etc.
inline ASTNode BinaryOpNode::Left() { return get()->Left; } // left operand
inline void BinaryOpNode::set_Left(ASTNode _v) { get()->Left = _v; } // left operand
inline ASTNode BinaryOpNode::Right() { return get()->Right; } // right operand
inline void BinaryOpNode::set_Right(ASTNode _v) { get()->Right = _v; } // right operand

inline CallNode::CallNode(std::shared_ptr<CallNodeStorage> stor) : ASTNode(stor) {}
inline CallNodeStorage* CallNode::get() const { return static_cast<CallNodeStorage*>(storage.get()); }
inline String CallNode::Function() { return get()->Function; } // function name
inline void CallNode::set_Function(String _v) { get()->Function = _v; } // function name
inline List<ASTNode> CallNode::Arguments() { return get()->Arguments; } // list of argument expressions
inline void CallNode::set_Arguments(List<ASTNode> _v) { get()->Arguments = _v; } // list of argument expressions

inline GroupNode::GroupNode(std::shared_ptr<GroupNodeStorage> stor) : ASTNode(stor) {}
inline GroupNodeStorage* GroupNode::get() const { return static_cast<GroupNodeStorage*>(storage.get()); }
inline ASTNode GroupNode::Expression() { return get()->Expression; }
inline void GroupNode::set_Expression(ASTNode _v) { get()->Expression = _v; }

inline ListNode::ListNode(std::shared_ptr<ListNodeStorage> stor) : ASTNode(stor) {}
inline ListNodeStorage* ListNode::get() const { return static_cast<ListNodeStorage*>(storage.get()); }
inline List<ASTNode> ListNode::Elements() { return get()->Elements; }
inline void ListNode::set_Elements(List<ASTNode> _v) { get()->Elements = _v; }

inline MapNode::MapNode(std::shared_ptr<MapNodeStorage> stor) : ASTNode(stor) {}
inline MapNodeStorage* MapNode::get() const { return static_cast<MapNodeStorage*>(storage.get()); }
inline List<ASTNode> MapNode::Keys() { return get()->Keys; }
inline void MapNode::set_Keys(List<ASTNode> _v) { get()->Keys = _v; }
inline List<ASTNode> MapNode::Values() { return get()->Values; }
inline void MapNode::set_Values(List<ASTNode> _v) { get()->Values = _v; }

inline IndexNode::IndexNode(std::shared_ptr<IndexNodeStorage> stor) : ASTNode(stor) {}
inline IndexNodeStorage* IndexNode::get() const { return static_cast<IndexNodeStorage*>(storage.get()); }
inline ASTNode IndexNode::Target() { return get()->Target; } // the list/map/string being indexed
inline void IndexNode::set_Target(ASTNode _v) { get()->Target = _v; } // the list/map/string being indexed
inline ASTNode IndexNode::Index() { return get()->Index; } // the index expression
inline void IndexNode::set_Index(ASTNode _v) { get()->Index = _v; } // the index expression

inline MemberNode::MemberNode(std::shared_ptr<MemberNodeStorage> stor) : ASTNode(stor) {}
inline MemberNodeStorage* MemberNode::get() const { return static_cast<MemberNodeStorage*>(storage.get()); }
inline ASTNode MemberNode::Target() { return get()->Target; } // the object being accessed
inline void MemberNode::set_Target(ASTNode _v) { get()->Target = _v; } // the object being accessed
inline String MemberNode::Member() { return get()->Member; } // the member name
inline void MemberNode::set_Member(String _v) { get()->Member = _v; } // the member name

inline MethodCallNode::MethodCallNode(std::shared_ptr<MethodCallNodeStorage> stor) : ASTNode(stor) {}
inline MethodCallNodeStorage* MethodCallNode::get() const { return static_cast<MethodCallNodeStorage*>(storage.get()); }
inline ASTNode MethodCallNode::Target() { return get()->Target; } // the object whose method is being called
inline void MethodCallNode::set_Target(ASTNode _v) { get()->Target = _v; } // the object whose method is being called
inline String MethodCallNode::Method() { return get()->Method; } // the method name
inline void MethodCallNode::set_Method(String _v) { get()->Method = _v; } // the method name
inline List<ASTNode> MethodCallNode::Arguments() { return get()->Arguments; } // list of argument expressions
inline void MethodCallNode::set_Arguments(List<ASTNode> _v) { get()->Arguments = _v; } // list of argument expressions

inline WhileNode::WhileNode(std::shared_ptr<WhileNodeStorage> stor) : ASTNode(stor) {}
inline WhileNodeStorage* WhileNode::get() const { return static_cast<WhileNodeStorage*>(storage.get()); }
inline ASTNode WhileNode::Condition() { return get()->Condition; } // the loop condition
inline void WhileNode::set_Condition(ASTNode _v) { get()->Condition = _v; } // the loop condition
inline List<ASTNode> WhileNode::Body() { return get()->Body; } // statements in the loop body
inline void WhileNode::set_Body(List<ASTNode> _v) { get()->Body = _v; } // statements in the loop body

inline IfNode::IfNode(std::shared_ptr<IfNodeStorage> stor) : ASTNode(stor) {}
inline IfNodeStorage* IfNode::get() const { return static_cast<IfNodeStorage*>(storage.get()); }
inline ASTNode IfNode::Condition() { return get()->Condition; } // the test expression
inline void IfNode::set_Condition(ASTNode _v) { get()->Condition = _v; } // the test expression
inline List<ASTNode> IfNode::ThenBody() { return get()->ThenBody; } // statements if condition is true
inline void IfNode::set_ThenBody(List<ASTNode> _v) { get()->ThenBody = _v; } // statements if condition is true
inline List<ASTNode> IfNode::ElseBody() { return get()->ElseBody; } // statements if condition is false (may contain IfNode for else-if)
inline void IfNode::set_ElseBody(List<ASTNode> _v) { get()->ElseBody = _v; } // statements if condition is false (may contain IfNode for else-if)

inline ForNode::ForNode(std::shared_ptr<ForNodeStorage> stor) : ASTNode(stor) {}
inline ForNodeStorage* ForNode::get() const { return static_cast<ForNodeStorage*>(storage.get()); }
inline String ForNode::Variable() { return get()->Variable; } // the loop variable name
inline void ForNode::set_Variable(String _v) { get()->Variable = _v; } // the loop variable name
inline ASTNode ForNode::Iterable() { return get()->Iterable; } // the expression to iterate over
inline void ForNode::set_Iterable(ASTNode _v) { get()->Iterable = _v; } // the expression to iterate over
inline List<ASTNode> ForNode::Body() { return get()->Body; } // statements in the loop body
inline void ForNode::set_Body(List<ASTNode> _v) { get()->Body = _v; } // statements in the loop body

inline BreakNode::BreakNode(std::shared_ptr<BreakNodeStorage> stor) : ASTNode(stor) {}
inline BreakNodeStorage* BreakNode::get() const { return static_cast<BreakNodeStorage*>(storage.get()); }

inline ContinueNode::ContinueNode(std::shared_ptr<ContinueNodeStorage> stor) : ASTNode(stor) {}
inline ContinueNodeStorage* ContinueNode::get() const { return static_cast<ContinueNodeStorage*>(storage.get()); }

inline FunctionNode::FunctionNode(std::shared_ptr<FunctionNodeStorage> stor) : ASTNode(stor) {}
inline FunctionNodeStorage* FunctionNode::get() const { return static_cast<FunctionNodeStorage*>(storage.get()); }
inline List<String> FunctionNode::ParamNames() { return get()->ParamNames; } // parameter names
inline void FunctionNode::set_ParamNames(List<String> _v) { get()->ParamNames = _v; } // parameter names
inline List<ASTNode> FunctionNode::Body() { return get()->Body; } // statements in the function body
inline void FunctionNode::set_Body(List<ASTNode> _v) { get()->Body = _v; } // statements in the function body

inline ReturnNode::ReturnNode(std::shared_ptr<ReturnNodeStorage> stor) : ASTNode(stor) {}
inline ReturnNodeStorage* ReturnNode::get() const { return static_cast<ReturnNodeStorage*>(storage.get()); }
inline ASTNode ReturnNode::Value() { return get()->Value; } // expression to return (null for bare return)
inline void ReturnNode::set_Value(ASTNode _v) { get()->Value = _v; } // expression to return (null for bare return)

} // end of namespace MiniScript
