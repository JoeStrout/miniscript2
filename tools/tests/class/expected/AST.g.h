// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AST.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

//----------------------------------------------------------------------
// Forward declaration of wrapper classes
//----------------------------------------------------------------------
struct ASTNode;
class ASTNodeStorage;
struct NumberNode;
class NumberNodeStorage;
struct BinaryOpNode;
class BinaryOpNodeStorage;

// 1. BASE CLASS DECLARATION COMES FIRST.  Abstract methods in C# are actually
// concrete here; they call through to the storage, and they're abstract there.

// Base class for all AST nodes.
struct ASTNode {
	protected: std::shared_ptr<ASTNodeStorage> storage;

  public:
    // Constructor for storage classes to use
    ASTNode(std::shared_ptr<ASTNodeStorage> stor) : storage(stor) {}

    // Default constructor creates a null node
    ASTNode() : storage(nullptr) {}

    // Check if null
    friend bool IsNull(ASTNode node) { return node.storage == nullptr; }

	// Type-safe cast helper
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(ASTNode node) {
		StorageType* stor = dynamic_cast<StorageType*>(node.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(node.storage);
	}

	// Each node type should override this to provide a string representation
	// Defined later, after storage classes are complete
	public: String Str();

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)
	// Defined later, after storage classes are complete
	public: ASTNode Simplify();
};

// Forward declaration of template function (needed for C++14 compatibility)
template<typename WrapperType, typename StorageType> WrapperType As(ASTNode node);

// 2. STORAGE CLASSES -- live on the heap, accessed via wrappers.  These will contain
// the actual method definitions (either inline, or in the .cpp file).

class ASTNodeStorage : public std::enable_shared_from_this<ASTNodeStorage> {
	public: virtual ~ASTNodeStorage() {}

	// Each node type should override this to provide a string representation
	public: virtual String Str() = 0;

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)
	public: virtual ASTNode Simplify() = 0;
};

// Number literal node (e.g., 42, 3.14)
class NumberNodeStorage : public ASTNodeStorage {
	public: Double value;

	public: NumberNodeStorage(Double value);

	public: String Str();

	public: ASTNode Simplify();
};

// Binary operator node (e.g., x + y, a * b)
class BinaryOpNodeStorage : public ASTNodeStorage {
	public: String op;       // Op.PLUS, etc.
	public: ASTNode left;    // left operand
	public: ASTNode right;   // right operand

	public: BinaryOpNodeStorage(String op, ASTNode left, ASTNode right);

	public: String Str();

	public: ASTNode Simplify();
};

// 3. DERIVED CLASS WRAPPER DECLARATIONS.  The wrapper methods can always be right
// inline with the class body, since they just call through to the storage classes,
// which are already declared above.

// Number literal node (e.g., 42, 3.14)
struct NumberNode : public ASTNode {
	public: NumberNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}

	public: NumberNode(Double value) : ASTNode(std::make_shared<NumberNodeStorage>(value)) {}

	private: NumberNodeStorage* get() { return static_cast<NumberNodeStorage*>(storage.get()); }

	public: Double value() { return get()->value; }
	public: void set_value(Double _v) { get()->value = _v; }
}; // end of class NumberNode

// Binary operator node (e.g., x + y, a * b)
struct BinaryOpNode : public ASTNode {
	public: BinaryOpNode(std::shared_ptr<ASTNodeStorage> stor) : ASTNode(stor) {}

	public: BinaryOpNode(String op, ASTNode left, ASTNode right) : ASTNode(std::make_shared<BinaryOpNodeStorage>(op, left, right)) {}

	private: BinaryOpNodeStorage* get() { return static_cast<BinaryOpNodeStorage*>(storage.get()); }

	public: String op() { return get()->op; }
	public: void set_op(String _v) { get()->op = _v; }
	public: ASTNode left() { return get()->left; }
	public: void set_left(ASTNode _v) { get()->left = _v; }
	public: ASTNode right() { return get()->right; }
	public: void set_right(ASTNode _v) { get()->right = _v; }
}; // end of class BinaryOpNode

// 4. INLINE METHOD DEFINITIONS that were not already defined in the struct/class body.
// (Only for methods marked with [MethodImpl(AggressiveInlining)] in the C# code.)

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
