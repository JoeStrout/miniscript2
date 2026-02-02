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
		auto stor = std::dynamic_pointer_cast<StorageType>(inst.storage);
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(stor); 
	}

	public: Precedence Prec();
	public: void set_Prec(Precedence _v);
}; // end of struct Parselet

template<typename WrapperType, typename StorageType> WrapperType As(Parselet inst);


// PrefixParselet: abstract base for parselets that handle tokens
// starting an expression (numbers, identifiers, unary operators).
struct PrefixParselet : public Parselet {
	PrefixParselet(std::shared_ptr<PrefixParseletStorage> stor);
	PrefixParselet() : Parselet() {}
	PrefixParselet(std::nullptr_t) : Parselet(nullptr) {}
	private: PrefixParseletStorage* get() const;

	public: ASTNode Parse(IParser& parser, Token token);
}; // end of struct PrefixParselet

template<typename WrapperType, typename StorageType> WrapperType As(PrefixParselet inst);


// InfixParselet: abstract base for parselets that handle infix operators.
struct InfixParselet : public Parselet {
	InfixParselet(std::shared_ptr<InfixParseletStorage> stor);
	InfixParselet() : Parselet() {}
	InfixParselet(std::nullptr_t) : Parselet(nullptr) {}
	private: InfixParseletStorage* get() const;

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of struct InfixParselet

template<typename WrapperType, typename StorageType> WrapperType As(InfixParselet inst);

class ParseletStorage : public std::enable_shared_from_this<ParseletStorage> {
	friend struct Parselet;
	public: virtual ~ParseletStorage() {}
	public: Precedence Prec;
}; // end of class ParseletStorage

class PrefixParseletStorage : public ParseletStorage {
	friend struct PrefixParselet;
	public: virtual ASTNode Parse(IParser& parser, Token token) = 0;
}; // end of class PrefixParseletStorage

class InfixParseletStorage : public ParseletStorage {
	friend struct InfixParselet;
	public: virtual ASTNode Parse(IParser& parser, ASTNode left, Token token) = 0;
}; // end of class InfixParseletStorage

class NumberParseletStorage : public PrefixParseletStorage {
	friend struct NumberParselet;
	public: NumberParseletStorage() {}
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class NumberParseletStorage

class StringParseletStorage : public PrefixParseletStorage {
	friend struct StringParselet;
	public: StringParseletStorage() {}
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class StringParseletStorage

class IdentifierParseletStorage : public PrefixParseletStorage {
	friend struct IdentifierParselet;
	public: IdentifierParseletStorage() {}
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class IdentifierParseletStorage

class UnaryOpParseletStorage : public PrefixParseletStorage {
	friend struct UnaryOpParselet;
	private: String _op;

	public: UnaryOpParseletStorage(String op, Precedence prec);

	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class UnaryOpParseletStorage

class GroupParseletStorage : public PrefixParseletStorage {
	friend struct GroupParselet;
	public: GroupParseletStorage() {}
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class GroupParseletStorage

class ListParseletStorage : public PrefixParseletStorage {
	friend struct ListParselet;
	public: ListParseletStorage() {}
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class ListParseletStorage

class MapParseletStorage : public PrefixParseletStorage {
	friend struct MapParselet;
	public: MapParseletStorage() {}
	public: ASTNode Parse(IParser& parser, Token token);
}; // end of class MapParseletStorage

class BinaryOpParseletStorage : public InfixParseletStorage {
	friend struct BinaryOpParselet;
	private: String _op;
	private: Boolean _rightAssoc;

	public: BinaryOpParseletStorage(String op, Precedence prec, Boolean rightAssoc);

	public: BinaryOpParseletStorage(String op, Precedence prec);

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class BinaryOpParseletStorage

class CallParseletStorage : public InfixParseletStorage {
	friend struct CallParselet;
	public: CallParseletStorage();

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class CallParseletStorage

class IndexParseletStorage : public InfixParseletStorage {
	friend struct IndexParselet;
	public: IndexParseletStorage();

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class IndexParseletStorage

class MemberParseletStorage : public InfixParseletStorage {
	friend struct MemberParselet;
	public: MemberParseletStorage();

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token);
}; // end of class MemberParseletStorage


// NumberParselet: handles number literals.
struct NumberParselet : public PrefixParselet {
	NumberParselet(std::shared_ptr<NumberParseletStorage> stor);
	NumberParselet() : PrefixParselet() {}
	NumberParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: NumberParseletStorage* get() const;

	public: static NumberParselet New() {
		return NumberParselet(std::make_shared<NumberParseletStorage>());
	}
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct NumberParselet


// StringParselet: handles string literals.
struct StringParselet : public PrefixParselet {
	StringParselet(std::shared_ptr<StringParseletStorage> stor);
	StringParselet() : PrefixParselet() {}
	StringParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: StringParseletStorage* get() const;

	public: static StringParselet New() {
		return StringParselet(std::make_shared<StringParseletStorage>());
	}
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct StringParselet


// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
struct IdentifierParselet : public PrefixParselet {
	IdentifierParselet(std::shared_ptr<IdentifierParseletStorage> stor);
	IdentifierParselet() : PrefixParselet() {}
	IdentifierParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: IdentifierParseletStorage* get() const;

	public: static IdentifierParselet New() {
		return IdentifierParselet(std::make_shared<IdentifierParseletStorage>());
	}
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct IdentifierParselet


// UnaryOpParselet: handles prefix unary operators like '-' and 'not'.
struct UnaryOpParselet : public PrefixParselet {
	UnaryOpParselet(std::shared_ptr<UnaryOpParseletStorage> stor);
	UnaryOpParselet() : PrefixParselet() {}
	UnaryOpParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: UnaryOpParseletStorage* get() const;

	private: String _op();
	private: void set__op(String _v);

	public: static UnaryOpParselet New(String op, Precedence prec) {
		return UnaryOpParselet(std::make_shared<UnaryOpParseletStorage>(op, prec));
	}

	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct UnaryOpParselet


// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
struct GroupParselet : public PrefixParselet {
	GroupParselet(std::shared_ptr<GroupParseletStorage> stor);
	GroupParselet() : PrefixParselet() {}
	GroupParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: GroupParseletStorage* get() const;

	public: static GroupParselet New() {
		return GroupParselet(std::make_shared<GroupParseletStorage>());
	}
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct GroupParselet


// ListParselet: handles list literals like '[1, 2, 3]'.
struct ListParselet : public PrefixParselet {
	ListParselet(std::shared_ptr<ListParseletStorage> stor);
	ListParselet() : PrefixParselet() {}
	ListParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: ListParseletStorage* get() const;

	public: static ListParselet New() {
		return ListParselet(std::make_shared<ListParseletStorage>());
	}
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct ListParselet


// MapParselet: handles map literals like '{"a": 1}'.
struct MapParselet : public PrefixParselet {
	MapParselet(std::shared_ptr<MapParseletStorage> stor);
	MapParselet() : PrefixParselet() {}
	MapParselet(std::nullptr_t) : PrefixParselet(nullptr) {}
	private: MapParseletStorage* get() const;

	public: static MapParselet New() {
		return MapParselet(std::make_shared<MapParseletStorage>());
	}
	public: ASTNode Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }
}; // end of struct MapParselet


// BinaryOpParselet: handles binary operators like '+', '-', '*', etc.
struct BinaryOpParselet : public InfixParselet {
	BinaryOpParselet(std::shared_ptr<BinaryOpParseletStorage> stor);
	BinaryOpParselet() : InfixParselet() {}
	BinaryOpParselet(std::nullptr_t) : InfixParselet(nullptr) {}
	private: BinaryOpParseletStorage* get() const;

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
	CallParselet(std::shared_ptr<CallParseletStorage> stor);
	CallParselet() : InfixParselet() {}
	CallParselet(std::nullptr_t) : InfixParselet(nullptr) {}
	private: CallParseletStorage* get() const;

	public: static CallParselet New() {
		return CallParselet(std::make_shared<CallParseletStorage>());
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct CallParselet


// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
struct IndexParselet : public InfixParselet {
	IndexParselet(std::shared_ptr<IndexParseletStorage> stor);
	IndexParselet() : InfixParselet() {}
	IndexParselet(std::nullptr_t) : InfixParselet(nullptr) {}
	private: IndexParseletStorage* get() const;

	public: static IndexParselet New() {
		return IndexParselet(std::make_shared<IndexParseletStorage>());
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct IndexParselet


// MemberParselet: handles member access like 'obj.field'.
struct MemberParselet : public InfixParselet {
	MemberParselet(std::shared_ptr<MemberParseletStorage> stor);
	MemberParselet() : InfixParselet() {}
	MemberParselet(std::nullptr_t) : InfixParselet(nullptr) {}
	private: MemberParseletStorage* get() const;

	public: static MemberParselet New() {
		return MemberParselet(std::make_shared<MemberParseletStorage>());
	}

	public: ASTNode Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }
}; // end of struct MemberParselet


// INLINE METHODS

inline ParseletStorage* Parselet::get() const { return static_cast<ParseletStorage*>(storage.get()); }
inline Precedence Parselet::Prec() { return get()->Prec; }
inline void Parselet::set_Prec(Precedence _v) { get()->Prec = _v; }

inline PrefixParselet::PrefixParselet(std::shared_ptr<PrefixParseletStorage> stor) : Parselet(stor) {}
inline PrefixParseletStorage* PrefixParselet::get() const { return static_cast<PrefixParseletStorage*>(storage.get()); }
inline ASTNode PrefixParselet::Parse(IParser& parser, Token token) { return get()->Parse(parser, token); }

inline InfixParselet::InfixParselet(std::shared_ptr<InfixParseletStorage> stor) : Parselet(stor) {}
inline InfixParseletStorage* InfixParselet::get() const { return static_cast<InfixParseletStorage*>(storage.get()); }
inline ASTNode InfixParselet::Parse(IParser& parser, ASTNode left, Token token) { return get()->Parse(parser, left, token); }

inline NumberParselet::NumberParselet(std::shared_ptr<NumberParseletStorage> stor) : PrefixParselet(stor) {}
inline NumberParseletStorage* NumberParselet::get() const { return static_cast<NumberParseletStorage*>(storage.get()); }

inline StringParselet::StringParselet(std::shared_ptr<StringParseletStorage> stor) : PrefixParselet(stor) {}
inline StringParseletStorage* StringParselet::get() const { return static_cast<StringParseletStorage*>(storage.get()); }

inline IdentifierParselet::IdentifierParselet(std::shared_ptr<IdentifierParseletStorage> stor) : PrefixParselet(stor) {}
inline IdentifierParseletStorage* IdentifierParselet::get() const { return static_cast<IdentifierParseletStorage*>(storage.get()); }

inline UnaryOpParselet::UnaryOpParselet(std::shared_ptr<UnaryOpParseletStorage> stor) : PrefixParselet(stor) {}
inline UnaryOpParseletStorage* UnaryOpParselet::get() const { return static_cast<UnaryOpParseletStorage*>(storage.get()); }
inline String UnaryOpParselet::_op() { return get()->_op; }
inline void UnaryOpParselet::set__op(String _v) { get()->_op = _v; }

inline GroupParselet::GroupParselet(std::shared_ptr<GroupParseletStorage> stor) : PrefixParselet(stor) {}
inline GroupParseletStorage* GroupParselet::get() const { return static_cast<GroupParseletStorage*>(storage.get()); }

inline ListParselet::ListParselet(std::shared_ptr<ListParseletStorage> stor) : PrefixParselet(stor) {}
inline ListParseletStorage* ListParselet::get() const { return static_cast<ListParseletStorage*>(storage.get()); }

inline MapParselet::MapParselet(std::shared_ptr<MapParseletStorage> stor) : PrefixParselet(stor) {}
inline MapParseletStorage* MapParselet::get() const { return static_cast<MapParseletStorage*>(storage.get()); }

inline BinaryOpParselet::BinaryOpParselet(std::shared_ptr<BinaryOpParseletStorage> stor) : InfixParselet(stor) {}
inline BinaryOpParseletStorage* BinaryOpParselet::get() const { return static_cast<BinaryOpParseletStorage*>(storage.get()); }
inline String BinaryOpParselet::_op() { return get()->_op; }
inline void BinaryOpParselet::set__op(String _v) { get()->_op = _v; }
inline Boolean BinaryOpParselet::_rightAssoc() { return get()->_rightAssoc; }
inline void BinaryOpParselet::set__rightAssoc(Boolean _v) { get()->_rightAssoc = _v; }

inline CallParselet::CallParselet(std::shared_ptr<CallParseletStorage> stor) : InfixParselet(stor) {}
inline CallParseletStorage* CallParselet::get() const { return static_cast<CallParseletStorage*>(storage.get()); }

inline IndexParselet::IndexParselet(std::shared_ptr<IndexParseletStorage> stor) : InfixParselet(stor) {}
inline IndexParseletStorage* IndexParselet::get() const { return static_cast<IndexParseletStorage*>(storage.get()); }

inline MemberParselet::MemberParselet(std::shared_ptr<MemberParseletStorage> stor) : InfixParselet(stor) {}
inline MemberParseletStorage* MemberParselet::get() const { return static_cast<MemberParseletStorage*>(storage.get()); }

} // end of namespace MiniScript
