// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AST.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

// FORWARD DECLARATIONS

struct ASTNode;
class ASTNodeStorage;
struct NumberNode;
class NumberNodeStorage;
struct BinaryOpNode;
class BinaryOpNodeStorage;

// DECLARATIONS


// Base class for all AST nodes.


// Number literal node (e.g., 42, 3.14)


// Binary operator node (e.g., x + y, a * b)

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

	public: String Str();
	public: ASTNode Simplify();
}; // end of struct ASTNode

template<typename WrapperType, typename StorageType> WrapperType As(ASTNode inst);

class ASTNodeStorage : public std::enable_shared_from_this<ASTNodeStorage> {
	public: virtual ~ASTNodeStorage() {}
	public: virtual String Str() = 0;
	public: virtual ASTNode Simplify() = 0;
}; // end of class ASTNodeStorage

class NumberNodeStorage : public ASTNodeStorage {
	public: Double value;
	public: NumberNodeStorage(Double value);
	public: String Str();
	public: ASTNode Simplify();
}; // end of class NumberNodeStorage

class BinaryOpNodeStorage : public ASTNodeStorage {
	public: String op; // Op.PLUS, etc.
	public: ASTNode left; // left operand
	public: ASTNode right; // right operand
	public: BinaryOpNodeStorage(String op, ASTNode left, ASTNode right);
	public: String Str();
	public: ASTNode Simplify();
}; // end of class BinaryOpNodeStorage

struct NumberNode : public ASTNode {
	public: NumberNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: NumberNodeStorage* get() { return static_cast<NumberNodeStorage*>(storage.get()); }
	public: Double value() { return get()->value; }
	public: void set_value(Double _v) { get()->value = _v; }
	public: NumberNode(Double value) : ASTNode(std::make_shared<NumberNodeStorage>(value)) {}
}; // end of struct NumberNode

struct BinaryOpNode : public ASTNode {
	public: BinaryOpNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}
	private: BinaryOpNodeStorage* get() { return static_cast<BinaryOpNodeStorage*>(storage.get()); }
	public: String op() { return get()->op; } // Op.PLUS, etc.
	public: void set_op(String _v) { get()->op = _v; } // Op.PLUS, etc.
	public: ASTNode left() { return get()->left; } // left operand
	public: void set_left(ASTNode _v) { get()->left = _v; } // left operand
	public: ASTNode right() { return get()->right; } // right operand
	public: void set_right(ASTNode _v) { get()->right = _v; } // right operand
	public: BinaryOpNode(String op, ASTNode left, ASTNode right) : ASTNode(std::make_shared<BinaryOpNodeStorage>(op, left, right)) {}
}; // end of struct BinaryOpNode


// INLINE METHODS

inline String ASTNode::Str() { return storage->Str(); }
inline ASTNode ASTNode::Simplify() { return storage->Simplify(); }

inline NumberNodeStorage::NumberNodeStorage(Double value) {
	this->value = value;
}
inline String NumberNodeStorage::Str() {
	return Interp("{}", value);
}
inline ASTNode NumberNodeStorage::Simplify() {
	return NumberNode(shared_from_this());
}

inline BinaryOpNodeStorage::BinaryOpNodeStorage(String op, ASTNode left, ASTNode right) {
	this->op = op;
	this->left = left;
	this->right = right;
}
inline String BinaryOpNodeStorage::Str() {
	return op + "(" + left.Str() + ", " + right.Str() + ")";
}

} // end of namespace MiniScript
