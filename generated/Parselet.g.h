// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parselet.cs

#pragma once
#include "core_includes.h"
// Parselet.cs - Mini-parsers for the Pratt parser
// Each parselet is responsible for parsing one type of expression construct.

#include "AST.g.h"
#include "Lexer.g.h"
#include "LangConstants.g.h"

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











// IParser interface: defines the methods that parselets need from the parser.
// This breaks the circular dependency between Parser and Parselet.
class IParser {
  public:
	virtual ~IParser() = default;
	virtual Boolean Check(TokenType type) = 0;
	virtual Boolean Match(TokenType type) = 0;
	virtual Token Consume() = 0;
	virtual Token Expect(TokenType type, String errorMessage) = 0;
	virtual ASTNode ParseExpression(Precedence minPrecedence) = 0;
	virtual void ReportError(String message) = 0;
}; // end of interface IParser







































// Base class for all parselets
struct Parselet {
	protected: std::shared_ptr<ParseletStorage> storage;
  public:
	Parselet(std::shared_ptr<ParseletStorage> stor) : storage(stor) {}
	Parselet() : storage(nullptr) {}
	Parselet(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Parselet& inst) { return inst.storage == nullptr; }
	private: ParseletStorage* get() const;
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(Parselet inst) {
		StorageType* stor = dynamic_cast<StorageType*>(inst.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(inst.storage);
	}

	public: Precedence Prec();
	public: void set_Prec(Precedence _v);
}; // end of struct Parselet

template<typename WrapperType, typename StorageType> WrapperType As(Parselet inst);


// PrefixParselet: abstract base for parselets that handle tokens
// starting an expression (numbers, identifiers, unary operators).
struct PrefixParselet : public Parselet {
	public: PrefixParselet(std::shared_ptr<ParseletStorage> stor) : Parselet(stor) {}
	private: PrefixParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of struct PrefixParselet

template<typename WrapperType, typename StorageType> WrapperType As(PrefixParselet inst);


// InfixParselet: abstract base for parselets that handle infix operators.
struct InfixParselet : public Parselet {
	public: InfixParselet(std::shared_ptr<ParseletStorage> stor) : Parselet(stor) {}
	private: InfixParseletStorage* get();
	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of struct InfixParselet

template<typename WrapperType, typename StorageType> WrapperType As(InfixParselet inst);

class ParseletStorage : public std::enable_shared_from_this<ParseletStorage> {
	friend struct Parselet;
	public: virtual ~ParseletStorage() {}
	public: Precedence Prec;
}; // end of class ParseletStorage


// PrefixParselet: abstract base for parselets that handle tokens
// starting an expression (numbers, identifiers, unary operators).
class PrefixParseletStorage : public ParseletStorage {
	friend struct PrefixParselet;
	public: virtual ASTNode Parse(IParser& parser, Token token) = 0;
}; // end of class PrefixParseletStorage


// InfixParselet: abstract base for parselets that handle infix operators.
class InfixParseletStorage : public ParseletStorage {
	friend struct InfixParselet;
	public: virtual ASTNode Parse(IParser& parser, ASTNode left, Token token) = 0;
}; // end of class InfixParseletStorage


// NumberParselet: handles number literals.
class NumberParseletStorage : public PrefixParseletStorage {
	friend struct NumberParselet;
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class NumberParseletStorage


// StringParselet: handles string literals.
class StringParseletStorage : public PrefixParseletStorage {
	friend struct StringParselet;
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class StringParseletStorage


// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
class IdentifierParseletStorage : public PrefixParseletStorage {
	friend struct IdentifierParselet;
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class IdentifierParseletStorage


// UnaryOpParselet: handles prefix unary operators like '-' and 'not'.
class UnaryOpParseletStorage : public PrefixParseletStorage {
	friend struct UnaryOpParselet;
	private: String _op;

	public: UnaryOpParseletStorage(String op, Precedence prec);

	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class UnaryOpParseletStorage


// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
class GroupParseletStorage : public PrefixParseletStorage {
	friend struct GroupParselet;
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class GroupParseletStorage


// ListParselet: handles list literals like '[1, 2, 3]'.
class ListParseletStorage : public PrefixParseletStorage {
	friend struct ListParselet;
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class ListParseletStorage


// MapParselet: handles map literals like '{"a": 1}'.
class MapParseletStorage : public PrefixParseletStorage {
	friend struct MapParselet;
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class MapParseletStorage


// BinaryOpParselet: handles binary operators like '+', '-', '*', etc.
class BinaryOpParseletStorage : public InfixParseletStorage {
	friend struct BinaryOpParselet;
	private: String _op;
	private: Boolean _rightAssoc;

	public: BinaryOpParseletStorage(String op, Precedence prec, Boolean rightAssoc);

	public: BinaryOpParseletStorage(String op, Precedence prec);

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class BinaryOpParseletStorage


// CallParselet: handles function calls like 'foo(x, y)' and method calls like 'obj.method(x)'.
class CallParseletStorage : public InfixParseletStorage {
	friend struct CallParselet;
	public: CallParseletStorage();

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class CallParseletStorage


// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
class IndexParseletStorage : public InfixParseletStorage {
	friend struct IndexParselet;
	public: IndexParseletStorage();

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class IndexParseletStorage


// MemberParselet: handles member access like 'obj.field'.
class MemberParseletStorage : public InfixParseletStorage {
	friend struct MemberParselet;
	public: MemberParseletStorage();

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class MemberParseletStorage


// NumberParselet: handles number literals.
struct NumberParselet : public PrefixParselet {
	public: NumberParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: NumberParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct NumberParselet


// StringParselet: handles string literals.
struct StringParselet : public PrefixParselet {
	public: StringParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: StringParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct StringParselet


// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
struct IdentifierParselet : public PrefixParselet {
	public: IdentifierParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: IdentifierParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct IdentifierParselet


// UnaryOpParselet: handles prefix unary operators like '-' and 'not'.
struct UnaryOpParselet : public PrefixParselet {
	public: UnaryOpParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: UnaryOpParseletStorage* get();
	private: String _op();
	private: void set__op(String _v);

	public: static UnaryOpParselet New(String op, Precedence prec) {
		return UnaryOpParselet(std::make_shared<UnaryOpParseletStorage>(op, prec));
	}

	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct UnaryOpParselet


// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
struct GroupParselet : public PrefixParselet {
	public: GroupParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: GroupParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct GroupParselet


// ListParselet: handles list literals like '[1, 2, 3]'.
struct ListParselet : public PrefixParselet {
	public: ListParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: ListParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct ListParselet


// MapParselet: handles map literals like '{"a": 1}'.
struct MapParselet : public PrefixParselet {
	public: MapParselet(std::shared_ptr<PrefixParseletStorage> stor) : PrefixParselet(stor) {}
	private: MapParseletStorage* get();
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct MapParselet


// BinaryOpParselet: handles binary operators like '+', '-', '*', etc.
struct BinaryOpParselet : public InfixParselet {
	public: BinaryOpParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: BinaryOpParseletStorage* get();
	private: String _op();
	private: void set__op(String _v);
	private: Boolean _rightAssoc();
	private: void set__rightAssoc(Boolean _v);

	public: static BinaryOpParselet New(String op, Precedence prec, Boolean rightAssoc) {
		return BinaryOpParselet(std::make_shared<BinaryOpParseletStorage>(op, prec, rightAssoc));
	}

	public: static BinaryOpParselet New(String op, Precedence prec) {
		return BinaryOpParselet(std::make_shared<BinaryOpParseletStorage>(op, prec));
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct BinaryOpParselet


// CallParselet: handles function calls like 'foo(x, y)' and method calls like 'obj.method(x)'.
struct CallParselet : public InfixParselet {
	public: CallParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: CallParseletStorage* get();
	public: static CallParselet New() {
		return CallParselet(std::make_shared<CallParseletStorage>());
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct CallParselet


// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
struct IndexParselet : public InfixParselet {
	public: IndexParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: IndexParseletStorage* get();
	public: static IndexParselet New() {
		return IndexParselet(std::make_shared<IndexParseletStorage>());
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct IndexParselet


// MemberParselet: handles member access like 'obj.field'.
struct MemberParselet : public InfixParselet {
	public: MemberParselet(std::shared_ptr<InfixParseletStorage> stor) : InfixParselet(stor) {}
	private: MemberParseletStorage* get();
	public: static MemberParselet New() {
		return MemberParselet(std::make_shared<MemberParseletStorage>());
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct MemberParselet


// INLINE METHODS

	ParseletStorage* Parselet::get() const { return static_cast<ParseletStorage*>(storage.get()); }
	Precedence Parselet::Prec() { return get()->Prec; }
	void Parselet::set_Prec(Precedence _v) { get()->Prec = _v; }

	PrefixParseletStorage* PrefixParselet::get() { return static_cast<PrefixParseletStorage*>(storage.get()); }
inline ASTNode PrefixParselet::Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }

	InfixParseletStorage* InfixParselet::get() { return static_cast<InfixParseletStorage*>(storage.get()); }
inline ASTNode InfixParselet::Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }

	NumberParseletStorage* NumberParselet::get() { return static_cast<NumberParseletStorage*>(storage.get()); }

	StringParseletStorage* StringParselet::get() { return static_cast<StringParseletStorage*>(storage.get()); }

	IdentifierParseletStorage* IdentifierParselet::get() { return static_cast<IdentifierParseletStorage*>(storage.get()); }

	UnaryOpParseletStorage* UnaryOpParselet::get() { return static_cast<UnaryOpParseletStorage*>(storage.get()); }
	String UnaryOpParselet::_op() { return get()->_op; }
	void UnaryOpParselet::set__op(String _v) { get()->_op = _v; }

	GroupParseletStorage* GroupParselet::get() { return static_cast<GroupParseletStorage*>(storage.get()); }

	ListParseletStorage* ListParselet::get() { return static_cast<ListParseletStorage*>(storage.get()); }

	MapParseletStorage* MapParselet::get() { return static_cast<MapParseletStorage*>(storage.get()); }

	BinaryOpParseletStorage* BinaryOpParselet::get() { return static_cast<BinaryOpParseletStorage*>(storage.get()); }
	String BinaryOpParselet::_op() { return get()->_op; }
	void BinaryOpParselet::set__op(String _v) { get()->_op = _v; }
	Boolean BinaryOpParselet::_rightAssoc() { return get()->_rightAssoc; }
	void BinaryOpParselet::set__rightAssoc(Boolean _v) { get()->_rightAssoc = _v; }

	CallParseletStorage* CallParselet::get() { return static_cast<CallParseletStorage*>(storage.get()); }

	IndexParseletStorage* IndexParselet::get() { return static_cast<IndexParseletStorage*>(storage.get()); }

	MemberParseletStorage* MemberParselet::get() { return static_cast<MemberParseletStorage*>(storage.get()); }

} // end of namespace MiniScript
