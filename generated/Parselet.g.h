// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parselet.cs

#pragma once
#include "core_includes.h"
// Parselet.cs - Mini-parsers for the Pratt parser
// Each parselet is responsible for parsing one type of expression construct.

#include "AST.g.h"
#include "Lexer.g.h"
#include "Parser.g.h"

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









// Precedence levels (higher precedence binds more strongly)
enum class Precedence : Int32 {
	NONE = 0,
	ASSIGNMENT = 1,
	OR = 2,
	AND = 3,
	EQUALITY = 4,        // == !=
	COMPARISON = 5,      // < > <= >=
	SUM = 6,             // + -
	PRODUCT = 7,         // * / %
	POWER = 8,           // ^
	UNARY = 9,           // - not
	CALL = 10,           // () []
	PRIMARY = 11
}; // end of enum Precedence








































// Base class for all parselets
struct Parselet {
	protected: std::shared_ptr<ParseletStorage> storage;
  public:
	Parselet(std::shared_ptr<ParseletStorage> stor) : storage(stor) {}
	Parselet() : storage(nullptr) {}
	friend bool IsNull(Parselet inst) { return inst.storage == nullptr; }
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(Parselet inst) {
		StorageType* stor = dynamic_cast<StorageType*>(inst.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(inst.storage);
	}

	public: Precedence Prec() { return get()->Prec; }
	public: void set_Prec(Precedence _v) { get()->Prec = _v; }
}; // end of struct Parselet

template<typename WrapperType, typename StorageType> WrapperType As(Parselet inst);


// PrefixParselet: abstract base for parselets that handle tokens
// starting an expression (numbers, identifiers, unary operators).
struct PrefixParselet {
	protected: std::shared_ptr<PrefixParseletStorage> storage;
  public:
	PrefixParselet(std::shared_ptr<PrefixParseletStorage> stor) : storage(stor) {}
	PrefixParselet() : storage(nullptr) {}
	friend bool IsNull(PrefixParselet inst) { return inst.storage == nullptr; }
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(PrefixParselet inst) {
		StorageType* stor = dynamic_cast<StorageType*>(inst.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(inst.storage);
	}

	public: ASTNode Parse(Parser parser, Token token);
}; // end of struct PrefixParselet

template<typename WrapperType, typename StorageType> WrapperType As(PrefixParselet inst);


// InfixParselet: abstract base for parselets that handle infix operators.
struct InfixParselet {
	protected: std::shared_ptr<InfixParseletStorage> storage;
  public:
	InfixParselet(std::shared_ptr<InfixParseletStorage> stor) : storage(stor) {}
	InfixParselet() : storage(nullptr) {}
	friend bool IsNull(InfixParselet inst) { return inst.storage == nullptr; }
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(InfixParselet inst) {
		StorageType* stor = dynamic_cast<StorageType*>(inst.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(inst.storage);
	}

	public: ASTNode Parse(Parser parser, ASTNode left, Token token);
}; // end of struct InfixParselet

template<typename WrapperType, typename StorageType> WrapperType As(InfixParselet inst);

class ParseletStorage : public std::enable_shared_from_this<ParseletStorage> {
	public: virtual ~ParseletStorage() {}
	public: Precedence Prec;
}; // end of class ParseletStorage

class PrefixParseletStorage : public std::enable_shared_from_this<PrefixParseletStorage> {
	public: virtual ~PrefixParseletStorage() {}
	public: virtual ASTNode Parse(Parser parser, Token token) = 0;
}; // end of class PrefixParseletStorage

class InfixParseletStorage : public std::enable_shared_from_this<InfixParseletStorage> {
	public: virtual ~InfixParseletStorage() {}
	public: virtual ASTNode Parse(Parser parser, ASTNode left, Token token) = 0;
}; // end of class InfixParseletStorage


// NumberParselet: handles number literals.
class NumberParseletStorage : public PrefixParseletStorage {
	public: ASTNode Parse(Parser parser, Token token);
}; // end of class NumberParseletStorage


// StringParselet: handles string literals.
class StringParseletStorage : public PrefixParseletStorage {
	public: ASTNode Parse(Parser parser, Token token);
}; // end of class StringParseletStorage


// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
class IdentifierParseletStorage : public PrefixParseletStorage {
	public: ASTNode Parse(Parser parser, Token token);
}; // end of class IdentifierParseletStorage


// UnaryOpParselet: handles prefix unary operators like '-' and 'not'.
class UnaryOpParseletStorage : public PrefixParseletStorage {
	private: String _op;

	public: UnaryOpParseletStorage(String op, Precedence prec);

	public: ASTNode Parse(Parser parser, Token token);
}; // end of class UnaryOpParseletStorage


// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
class GroupParseletStorage : public PrefixParseletStorage {
	public: ASTNode Parse(Parser parser, Token token);
}; // end of class GroupParseletStorage


// ListParselet: handles list literals like '[1, 2, 3]'.
class ListParseletStorage : public PrefixParseletStorage {
	public: ASTNode Parse(Parser parser, Token token);
}; // end of class ListParseletStorage


// MapParselet: handles map literals like '{"a": 1}'.
class MapParseletStorage : public PrefixParseletStorage {
	public: ASTNode Parse(Parser parser, Token token);
}; // end of class MapParseletStorage


// BinaryOpParselet: handles binary operators like '+', '-', '*', etc.
class BinaryOpParseletStorage : public InfixParseletStorage {
	private: String _op;
	private: Boolean _rightAssoc;

	public: BinaryOpParseletStorage(String op, Precedence prec, Boolean rightAssoc);

	public: BinaryOpParseletStorage(String op, Precedence prec);

	public: ASTNode Parse(Parser parser, ASTNode left, Token token);
}; // end of class BinaryOpParseletStorage


// CallParselet: handles function calls like 'foo(x, y)' and method calls like 'obj.method(x)'.
class CallParseletStorage : public InfixParseletStorage {
	public: CallParseletStorage();

	public: ASTNode Parse(Parser parser, ASTNode left, Token token);
}; // end of class CallParseletStorage


// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
class IndexParseletStorage : public InfixParseletStorage {
	public: IndexParseletStorage();

	public: ASTNode Parse(Parser parser, ASTNode left, Token token);
}; // end of class IndexParseletStorage


// MemberParselet: handles member access like 'obj.field'.
class MemberParseletStorage : public InfixParseletStorage {
	public: MemberParseletStorage();

	public: ASTNode Parse(Parser parser, ASTNode left, Token token);
}; // end of class MemberParseletStorage


// NumberParselet: handles number literals.
struct NumberParselet : public PrefixParselet {
	public: NumberParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: NumberParseletStorage* get() { return static_cast<NumberParseletStorage*>(storage.get()); }
	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct NumberParselet


// StringParselet: handles string literals.
struct StringParselet : public PrefixParselet {
	public: StringParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: StringParseletStorage* get() { return static_cast<StringParseletStorage*>(storage.get()); }
	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct StringParselet


// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
struct IdentifierParselet : public PrefixParselet {
	public: IdentifierParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: IdentifierParseletStorage* get() { return static_cast<IdentifierParseletStorage*>(storage.get()); }
	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct IdentifierParselet


// UnaryOpParselet: handles prefix unary operators like '-' and 'not'.
struct UnaryOpParselet : public PrefixParselet {
	public: UnaryOpParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: UnaryOpParseletStorage* get() { return static_cast<UnaryOpParseletStorage*>(storage.get()); }
	private: String _op() { return get()->_op; }
	private: void set__op(String _v) { get()->_op = _v; }

	public: static UnaryOpParselet New(String op, Precedence prec) {
		return UnaryOpParselet(std::make_shared<UnaryOpParseletStorage>(op, prec));
	}

	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct UnaryOpParselet


// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
struct GroupParselet : public PrefixParselet {
	public: GroupParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: GroupParseletStorage* get() { return static_cast<GroupParseletStorage*>(storage.get()); }
	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct GroupParselet


// ListParselet: handles list literals like '[1, 2, 3]'.
struct ListParselet : public PrefixParselet {
	public: ListParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: ListParseletStorage* get() { return static_cast<ListParseletStorage*>(storage.get()); }
	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct ListParselet


// MapParselet: handles map literals like '{"a": 1}'.
struct MapParselet : public PrefixParselet {
	public: MapParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: MapParseletStorage* get() { return static_cast<MapParseletStorage*>(storage.get()); }
	public: ASTNode Parse(Parser parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct MapParselet


// BinaryOpParselet: handles binary operators like '+', '-', '*', etc.
struct BinaryOpParselet : public InfixParselet {
	public: BinaryOpParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: BinaryOpParseletStorage* get() { return static_cast<BinaryOpParseletStorage*>(storage.get()); }
	private: String _op() { return get()->_op; }
	private: void set__op(String _v) { get()->_op = _v; }
	private: Boolean _rightAssoc() { return get()->_rightAssoc; }
	private: void set__rightAssoc(Boolean _v) { get()->_rightAssoc = _v; }

	public: static BinaryOpParselet New(String op, Precedence prec, Boolean rightAssoc) {
		return BinaryOpParselet(std::make_shared<BinaryOpParseletStorage>(op, prec, rightAssoc));
	}

	public: static BinaryOpParselet New(String op, Precedence prec) {
		return BinaryOpParselet(std::make_shared<BinaryOpParseletStorage>(op, prec));
	}

	public: ASTNode Parse(Parser parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct BinaryOpParselet


// CallParselet: handles function calls like 'foo(x, y)' and method calls like 'obj.method(x)'.
struct CallParselet : public InfixParselet {
	public: CallParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: CallParseletStorage* get() { return static_cast<CallParseletStorage*>(storage.get()); }
	public: static CallParselet New() {
		return CallParselet(std::make_shared<CallParseletStorage>());
	}

	public: ASTNode Parse(Parser parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct CallParselet


// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
struct IndexParselet : public InfixParselet {
	public: IndexParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: IndexParseletStorage* get() { return static_cast<IndexParseletStorage*>(storage.get()); }
	public: static IndexParselet New() {
		return IndexParselet(std::make_shared<IndexParseletStorage>());
	}

	public: ASTNode Parse(Parser parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct IndexParselet


// MemberParselet: handles member access like 'obj.field'.
struct MemberParselet : public InfixParselet {
	public: MemberParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: MemberParseletStorage* get() { return static_cast<MemberParseletStorage*>(storage.get()); }
	public: static MemberParselet New() {
		return MemberParselet(std::make_shared<MemberParseletStorage>());
	}

	public: ASTNode Parse(Parser parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct MemberParselet


// INLINE METHODS

inline ASTNode PrefixParselet::Parse(Parser parser, Token token) { return storage->Parse(parser, token); }

inline ASTNode InfixParselet::Parse(Parser parser, ASTNode left, Token token) { return storage->Parse(parser, left, token); }

} // end of namespace MiniScript
