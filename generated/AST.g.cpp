// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AST.cs

#include "AST.g.h"
#include "StringUtils.g.h"
#include "CS_Math.h"
#include <cmath>

namespace MiniScript {


const String Op::PLUS = "PLUS";
const String Op::MINUS = "MINUS";
const String Op::TIMES = "TIMES";
const String Op::DIVIDE = "DIVIDE";
const String Op::MOD = "MOD";
const String Op::POWER = "POWER";
const String Op::ADDRESS_OF = "ADDRESS_OF";
const String Op::EQUALS = "EQUALS";
const String Op::NOT_EQUAL = "NOT_EQUAL";
const String Op::LESS_THAN = "LESS_THAN";
const String Op::GREATER_THAN = "GREATER_THAN";
const String Op::LESS_EQUAL = "LESS_EQUAL";
const String Op::GREATER_EQUAL = "GREATER_EQUAL";
const String Op::AND = "AND";
const String Op::OR = "OR";
const String Op::NOT = "NOT";
const String Op::NEW = "NEW";
const String Op::ISA = "ISA";






NumberNodeStorage::NumberNodeStorage(Double value) {
	Value = value;
}
String NumberNodeStorage::ToStr() {
	return Interp("{}", Value);
}
ASTNode NumberNodeStorage::Simplify() {
	NumberNode _this(std::static_pointer_cast<NumberNodeStorage>(shared_from_this()));
	return _this;
}
Int32 NumberNodeStorage::Accept(IASTVisitor& visitor) {
	NumberNode _this(std::static_pointer_cast<NumberNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


StringNodeStorage::StringNodeStorage(String value) {
	Value = value;
}
String StringNodeStorage::ToStr() {
	return "\"" + Value + "\"";
}
ASTNode StringNodeStorage::Simplify() {
	StringNode _this(std::static_pointer_cast<StringNodeStorage>(shared_from_this()));
	return _this;
}
Int32 StringNodeStorage::Accept(IASTVisitor& visitor) {
	StringNode _this(std::static_pointer_cast<StringNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


IdentifierNodeStorage::IdentifierNodeStorage(String name) {
	Name = name;
}
String IdentifierNodeStorage::ToStr() {
	return Name;
}
ASTNode IdentifierNodeStorage::Simplify() {
	IdentifierNode _this(std::static_pointer_cast<IdentifierNodeStorage>(shared_from_this()));
	return _this;
}
Int32 IdentifierNodeStorage::Accept(IASTVisitor& visitor) {
	IdentifierNode _this(std::static_pointer_cast<IdentifierNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


AssignmentNodeStorage::AssignmentNodeStorage(String variable, ASTNode value) {
	Variable = variable;
	Value = value;
}
String AssignmentNodeStorage::ToStr() {
	return Variable + " = " + Value.ToStr();
}
ASTNode AssignmentNodeStorage::Simplify() {
	ASTNode simplifiedValue = Value.Simplify();
	return  AssignmentNode::New(Variable, simplifiedValue);
}
Int32 AssignmentNodeStorage::Accept(IASTVisitor& visitor) {
	AssignmentNode _this(std::static_pointer_cast<AssignmentNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


IndexedAssignmentNodeStorage::IndexedAssignmentNodeStorage(ASTNode target, ASTNode index, ASTNode value) {
	Target = target;
	Index = index;
	Value = value;
}
String IndexedAssignmentNodeStorage::ToStr() {
	return Target.ToStr() + "[" + Index.ToStr() + "] = " + Value.ToStr();
}
ASTNode IndexedAssignmentNodeStorage::Simplify() {
	return  IndexedAssignmentNode::New(Target.Simplify(), Index.Simplify(), Value.Simplify());
}
Int32 IndexedAssignmentNodeStorage::Accept(IASTVisitor& visitor) {
	IndexedAssignmentNode _this(std::static_pointer_cast<IndexedAssignmentNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


UnaryOpNodeStorage::UnaryOpNodeStorage(String op, ASTNode operand) {
	Op = op;
	Operand = operand;
}
String UnaryOpNodeStorage::ToStr() {
	return Op + "(" + Operand.ToStr() + ")";
}
ASTNode UnaryOpNodeStorage::Simplify() {
	ASTNode simplifiedOperand = Operand.Simplify();

	// If operand is a constant, compute the result
	NumberNode num = As<NumberNode, NumberNodeStorage>(simplifiedOperand);
	if (!IsNull(num)) {
		if (Op == MiniScript::Op::MINUS) {
			return  NumberNode::New(-num.Value());
		} else if (Op == MiniScript::Op::NOT) {
			// Fuzzy logic NOT: 1 - AbsClamp01(value)
			return  NumberNode::New(1.0 - AbsClamp01(num.Value()));
		}
	}

	// Otherwise return unary op with simplified operand
	return  UnaryOpNode::New(Op, simplifiedOperand);
}
Int32 UnaryOpNodeStorage::Accept(IASTVisitor& visitor) {
	UnaryOpNode _this(std::static_pointer_cast<UnaryOpNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


BinaryOpNodeStorage::BinaryOpNodeStorage(String op, ASTNode left, ASTNode right) {
	Op = op;
	Left = left;
	Right = right;
}
String BinaryOpNodeStorage::ToStr() {
	return Op + "(" + Left.ToStr() + ", " + Right.ToStr() + ")";
}
ASTNode BinaryOpNodeStorage::Simplify() {
	ASTNode simplifiedLeft = Left.Simplify();
	ASTNode simplifiedRight = Right.Simplify();

	// If both operands are numeric constants, compute the result
	NumberNode leftNum = As<NumberNode, NumberNodeStorage>(simplifiedLeft);
	NumberNode rightNum = As<NumberNode, NumberNodeStorage>(simplifiedRight);
	if (!IsNull(leftNum) && !IsNull(rightNum)) {
		if (Op == MiniScript::Op::PLUS) {
			return  NumberNode::New(leftNum.Value() + rightNum.Value());
		} else if (Op == MiniScript::Op::MINUS) {
			return  NumberNode::New(leftNum.Value() - rightNum.Value());
		} else if (Op == MiniScript::Op::TIMES) {
			return  NumberNode::New(leftNum.Value() * rightNum.Value());
		} else if (Op == MiniScript::Op::DIVIDE) {
			return  NumberNode::New(leftNum.Value() / rightNum.Value());
		} else if (Op == MiniScript::Op::MOD) {
			Double result = fmod(leftNum.Value(), rightNum.Value());
			return  NumberNode::New(result);
		} else if (Op == MiniScript::Op::POWER) {
			return  NumberNode::New(Math::Pow(leftNum.Value(), rightNum.Value()));
		} else if (Op == MiniScript::Op::EQUALS) {
			return  NumberNode::New(leftNum.Value() == rightNum.Value() ? 1 : 0);
		} else if (Op == MiniScript::Op::NOT_EQUAL) {
			return  NumberNode::New(leftNum.Value() != rightNum.Value() ? 1 : 0);
		} else if (Op == MiniScript::Op::LESS_THAN) {
			return  NumberNode::New(leftNum.Value() < rightNum.Value() ? 1 : 0);
		} else if (Op == MiniScript::Op::GREATER_THAN) {
			return  NumberNode::New(leftNum.Value() > rightNum.Value() ? 1 : 0);
		} else if (Op == MiniScript::Op::LESS_EQUAL) {
			return  NumberNode::New(leftNum.Value() <= rightNum.Value() ? 1 : 0);
		} else if (Op == MiniScript::Op::GREATER_EQUAL) {
			return  NumberNode::New(leftNum.Value() >= rightNum.Value() ? 1 : 0);
		} else if (Op == MiniScript::Op::AND) {
			// Fuzzy logic AND: AbsClamp01(a * b)
			Double a = leftNum.Value();
			Double b = rightNum.Value();
			return  NumberNode::New(AbsClamp01(a * b));
		} else if (Op == MiniScript::Op::OR) {
			// Fuzzy logic OR: AbsClamp01(a + b - a*b)
			Double a = leftNum.Value();
			Double b = rightNum.Value();
			return  NumberNode::New(AbsClamp01(a + b - a * b));
		}
	}

	// Otherwise return binary op with simplified operands
	return  BinaryOpNode::New(Op, simplifiedLeft, simplifiedRight);
}
Int32 BinaryOpNodeStorage::Accept(IASTVisitor& visitor) {
	BinaryOpNode _this(std::static_pointer_cast<BinaryOpNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


CallNodeStorage::CallNodeStorage(String function, List<ASTNode> arguments) {
	Function = function;
	Arguments = arguments;
}
CallNodeStorage::CallNodeStorage(String function) {
	Function = function;
	Arguments =  List<ASTNode>::New();
}
String CallNodeStorage::ToStr() {
	String result = Function + "(";
	for (Int32 i = 0; i < Arguments.Count(); i++) {
		if (i > 0) result = result + ", ";
		result = result + Arguments[i].ToStr();
	}
	result = result + ")";
	return result;
}
ASTNode CallNodeStorage::Simplify() {
	List<ASTNode> simplifiedArgs =  List<ASTNode>::New();
	for (Int32 i = 0; i < Arguments.Count(); i++) {
		simplifiedArgs.Add(Arguments[i].Simplify());
	}
	return  CallNode::New(Function, simplifiedArgs);
}
Int32 CallNodeStorage::Accept(IASTVisitor& visitor) {
	CallNode _this(std::static_pointer_cast<CallNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


GroupNodeStorage::GroupNodeStorage(ASTNode expression) {
	Expression = expression;
}
String GroupNodeStorage::ToStr() {
	return "(" + Expression.ToStr() + ")";
}
ASTNode GroupNodeStorage::Simplify() {
	// Groups don't affect value, just return simplified child
	return Expression.Simplify();
}
Int32 GroupNodeStorage::Accept(IASTVisitor& visitor) {
	GroupNode _this(std::static_pointer_cast<GroupNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


ListNodeStorage::ListNodeStorage(List<ASTNode> elements) {
	Elements = elements;
}
ListNodeStorage::ListNodeStorage() {
	Elements =  List<ASTNode>::New();
}
String ListNodeStorage::ToStr() {
	String result = "[";
	for (Int32 i = 0; i < Elements.Count(); i++) {
		if (i > 0) result = result + ", ";
		result = result + Elements[i].ToStr();
	}
	result = result + "]";
	return result;
}
ASTNode ListNodeStorage::Simplify() {
	List<ASTNode> simplifiedElements =  List<ASTNode>::New();
	for (Int32 i = 0; i < Elements.Count(); i++) {
		simplifiedElements.Add(Elements[i].Simplify());
	}
	return  ListNode::New(simplifiedElements);
}
Int32 ListNodeStorage::Accept(IASTVisitor& visitor) {
	ListNode _this(std::static_pointer_cast<ListNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


MapNodeStorage::MapNodeStorage(List<ASTNode> keys, List<ASTNode> values) {
	Keys = keys;
	Values = values;
}
MapNodeStorage::MapNodeStorage() {
	Keys =  List<ASTNode>::New();
	Values =  List<ASTNode>::New();
}
String MapNodeStorage::ToStr() {
	String result = "{";
	for (Int32 i = 0; i < Keys.Count(); i++) {
		if (i > 0) result = result + ", ";
		result = result + Keys[i].ToStr() + ": " + Values[i].ToStr();
	}
	result = result + "}";
	return result;
}
ASTNode MapNodeStorage::Simplify() {
	List<ASTNode> simplifiedKeys =  List<ASTNode>::New();
	List<ASTNode> simplifiedValues =  List<ASTNode>::New();
	for (Int32 i = 0; i < Keys.Count(); i++) {
		simplifiedKeys.Add(Keys[i].Simplify());
		simplifiedValues.Add(Values[i].Simplify());
	}
	return  MapNode::New(simplifiedKeys, simplifiedValues);
}
Int32 MapNodeStorage::Accept(IASTVisitor& visitor) {
	MapNode _this(std::static_pointer_cast<MapNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


IndexNodeStorage::IndexNodeStorage(ASTNode target, ASTNode index) {
	Target = target;
	Index = index;
}
String IndexNodeStorage::ToStr() {
	return Target.ToStr() + "[" + Index.ToStr() + "]";
}
ASTNode IndexNodeStorage::Simplify() {
	return  IndexNode::New(Target.Simplify(), Index.Simplify());
}
Int32 IndexNodeStorage::Accept(IASTVisitor& visitor) {
	IndexNode _this(std::static_pointer_cast<IndexNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


MemberNodeStorage::MemberNodeStorage(ASTNode target, String member) {
	Target = target;
	Member = member;
}
String MemberNodeStorage::ToStr() {
	return Target.ToStr() + "." + Member;
}
ASTNode MemberNodeStorage::Simplify() {
	return  MemberNode::New(Target.Simplify(), Member);
}
Int32 MemberNodeStorage::Accept(IASTVisitor& visitor) {
	MemberNode _this(std::static_pointer_cast<MemberNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


MethodCallNodeStorage::MethodCallNodeStorage(ASTNode target, String method, List<ASTNode> arguments) {
	Target = target;
	Method = method;
	Arguments = arguments;
}
String MethodCallNodeStorage::ToStr() {
	String result = Target.ToStr() + "." + Method + "(";
	for (Int32 i = 0; i < Arguments.Count(); i++) {
		if (i > 0) result = result + ", ";
		result = result + Arguments[i].ToStr();
	}
	result = result + ")";
	return result;
}
ASTNode MethodCallNodeStorage::Simplify() {
	List<ASTNode> simplifiedArgs =  List<ASTNode>::New();
	for (Int32 i = 0; i < Arguments.Count(); i++) {
		simplifiedArgs.Add(Arguments[i].Simplify());
	}
	return  MethodCallNode::New(Target.Simplify(), Method, simplifiedArgs);
}
Int32 MethodCallNodeStorage::Accept(IASTVisitor& visitor) {
	MethodCallNode _this(std::static_pointer_cast<MethodCallNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


ExprCallNodeStorage::ExprCallNodeStorage(ASTNode function, List<ASTNode> arguments) {
	Function = function;
	Arguments = arguments;
}
String ExprCallNodeStorage::ToStr() {
	String result = Function.ToStr() + "(";
	for (Int32 i = 0; i < Arguments.Count(); i++) {
		if (i > 0) result = result + ", ";
		result = result + Arguments[i].ToStr();
	}
	result = result + ")";
	return result;
}
ASTNode ExprCallNodeStorage::Simplify() {
	List<ASTNode> simplifiedArgs =  List<ASTNode>::New();
	for (Int32 i = 0; i < Arguments.Count(); i++) {
		simplifiedArgs.Add(Arguments[i].Simplify());
	}
	return  ExprCallNode::New(Function.Simplify(), simplifiedArgs);
}
Int32 ExprCallNodeStorage::Accept(IASTVisitor& visitor) {
	ExprCallNode _this(std::static_pointer_cast<ExprCallNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


WhileNodeStorage::WhileNodeStorage(ASTNode condition, List<ASTNode> body) {
	Condition = condition;
	Body = body;
}
String WhileNodeStorage::ToStr() {
	String result = "while " + Condition.ToStr() + "\n";
	for (Int32 i = 0; i < Body.Count(); i++) {
		result = result + "  " + Body[i].ToStr() + "\n";
	}
	result = result + "end while";
	return result;
}
ASTNode WhileNodeStorage::Simplify() {
	ASTNode simplifiedCondition = Condition.Simplify();
	List<ASTNode> simplifiedBody =  List<ASTNode>::New();
	for (Int32 i = 0; i < Body.Count(); i++) {
		simplifiedBody.Add(Body[i].Simplify());
	}
	return  WhileNode::New(simplifiedCondition, simplifiedBody);
}
Int32 WhileNodeStorage::Accept(IASTVisitor& visitor) {
	WhileNode _this(std::static_pointer_cast<WhileNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


IfNodeStorage::IfNodeStorage(ASTNode condition, List<ASTNode> thenBody, List<ASTNode> elseBody) {
	Condition = condition;
	ThenBody = thenBody;
	ElseBody = elseBody;
}
String IfNodeStorage::ToStr() {
	String result = "if " + Condition.ToStr() + " then\n";
	for (Int32 i = 0; i < ThenBody.Count(); i++) {
		result = result + "  " + ThenBody[i].ToStr() + "\n";
	}
	if (ElseBody.Count() > 0) {
		result = result + "else\n";
		for (Int32 i = 0; i < ElseBody.Count(); i++) {
			result = result + "  " + ElseBody[i].ToStr() + "\n";
		}
	}
	result = result + "end if";
	return result;
}
ASTNode IfNodeStorage::Simplify() {
	ASTNode simplifiedCondition = Condition.Simplify();
	List<ASTNode> simplifiedThenBody =  List<ASTNode>::New();
	for (Int32 i = 0; i < ThenBody.Count(); i++) {
		simplifiedThenBody.Add(ThenBody[i].Simplify());
	}
	List<ASTNode> simplifiedElseBody =  List<ASTNode>::New();
	for (Int32 i = 0; i < ElseBody.Count(); i++) {
		simplifiedElseBody.Add(ElseBody[i].Simplify());
	}
	return  IfNode::New(simplifiedCondition, simplifiedThenBody, simplifiedElseBody);
}
Int32 IfNodeStorage::Accept(IASTVisitor& visitor) {
	IfNode _this(std::static_pointer_cast<IfNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


ForNodeStorage::ForNodeStorage(String variable, ASTNode iterable, List<ASTNode> body) {
	Variable = variable;
	Iterable = iterable;
	Body = body;
}
String ForNodeStorage::ToStr() {
	String result = "for " + Variable + " in " + Iterable.ToStr() + "\n";
	for (Int32 i = 0; i < Body.Count(); i++) {
		result = result + "  " + Body[i].ToStr() + "\n";
	}
	result = result + "end for";
	return result;
}
ASTNode ForNodeStorage::Simplify() {
	ASTNode simplifiedIterable = Iterable.Simplify();
	List<ASTNode> simplifiedBody =  List<ASTNode>::New();
	for (Int32 i = 0; i < Body.Count(); i++) {
		simplifiedBody.Add(Body[i].Simplify());
	}
	return  ForNode::New(Variable, simplifiedIterable, simplifiedBody);
}
Int32 ForNodeStorage::Accept(IASTVisitor& visitor) {
	ForNode _this(std::static_pointer_cast<ForNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


BreakNodeStorage::BreakNodeStorage() {
}
String BreakNodeStorage::ToStr() {
	return "break";
}
ASTNode BreakNodeStorage::Simplify() {
	BreakNode _this(std::static_pointer_cast<BreakNodeStorage>(shared_from_this()));
	return _this;
}
Int32 BreakNodeStorage::Accept(IASTVisitor& visitor) {
	BreakNode _this(std::static_pointer_cast<BreakNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


ContinueNodeStorage::ContinueNodeStorage() {
}
String ContinueNodeStorage::ToStr() {
	return "continue";
}
ASTNode ContinueNodeStorage::Simplify() {
	ContinueNode _this(std::static_pointer_cast<ContinueNodeStorage>(shared_from_this()));
	return _this;
}
Int32 ContinueNodeStorage::Accept(IASTVisitor& visitor) {
	ContinueNode _this(std::static_pointer_cast<ContinueNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


FunctionNodeStorage::FunctionNodeStorage(List<String> paramNames, List<ASTNode> paramDefaults, List<ASTNode> body) {
	ParamNames = paramNames;
	ParamDefaults = paramDefaults;
	Body = body;
}
String FunctionNodeStorage::ToStr() {
	String result = "function(";
	for (Int32 i = 0; i < ParamNames.Count(); i++) {
		if (i > 0) result = result + ", ";
		result = result + ParamNames[i];
		ASTNode def = ParamDefaults[i];
		if (!IsNull(def)) {
			result = result + "=" + def.ToStr();
		}
	}
	result = result + ")\n";
	for (Int32 i = 0; i < Body.Count(); i++) {
		result = result + "  " + Body[i].ToStr() + "\n";
	}
	result = result + "end function";
	return result;
}
ASTNode FunctionNodeStorage::Simplify() {
	List<ASTNode> simplifiedDefaults =  List<ASTNode>::New();
	for (Int32 i = 0; i < ParamDefaults.Count(); i++) {
		ASTNode def = ParamDefaults[i];
		if (!IsNull(def)) {
			simplifiedDefaults.Add(def.Simplify());
		} else {
			simplifiedDefaults.Add(nullptr);
		}
	}
	List<ASTNode> simplifiedBody =  List<ASTNode>::New();
	for (Int32 i = 0; i < Body.Count(); i++) {
		simplifiedBody.Add(Body[i].Simplify());
	}
	return  FunctionNode::New(ParamNames, simplifiedDefaults, simplifiedBody);
}
Int32 FunctionNodeStorage::Accept(IASTVisitor& visitor) {
	FunctionNode _this(std::static_pointer_cast<FunctionNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


ReturnNodeStorage::ReturnNodeStorage(ASTNode value) {
	Value = value;
}
String ReturnNodeStorage::ToStr() {
	if (!IsNull(Value)) return "return " + Value.ToStr();
	return "return";
}
ASTNode ReturnNodeStorage::Simplify() {
	ReturnNode _this(std::static_pointer_cast<ReturnNodeStorage>(shared_from_this()));
	if (!IsNull(Value)) return  ReturnNode::New(Value.Simplify());
	return _this;
}
Int32 ReturnNodeStorage::Accept(IASTVisitor& visitor) {
	ReturnNode _this(std::static_pointer_cast<ReturnNodeStorage>(shared_from_this()));
	return visitor.Visit(_this);
}


} // end of namespace MiniScript
