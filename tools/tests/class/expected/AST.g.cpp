// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ast.cs

#include "AST.g.h"


namespace MiniScript {


ASTNode BinaryOpNodeStorage::Simplify() {
	ASTNode simplifiedLeft = left.Simplify();
	ASTNode simplifiedRight = right.Simplify();

	// If both operands are constants, compute the result
	NumberNode leftNum = As<NumberNode, NumberNodeStorage>(simplifiedLeft);
	NumberNode rightNum = As<NumberNode, NumberNodeStorage>(simplifiedRight);
	if (!IsNull(leftNum) && !IsNull(rightNum)) {
		if (op == "PLUS") {
			return NumberNode(leftNum.value() + rightNum.value());
		} else if (op == "MINUS") {
			return NumberNode(leftNum.value() - rightNum.value());
		} else if (op == "TIMES") {
			return NumberNode(leftNum.value() * rightNum.value());
		}
	}

	// Otherwise return binary op with simplified operands
	return BinaryOpNode(op, simplifiedLeft, simplifiedRight);
}


} // end of namespace MiniScript
