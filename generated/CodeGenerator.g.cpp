// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeGenerator.cs

#include "CodeGenerator.g.h"
#include "CS_Math.h"
#include "gc.h"

namespace MiniScript {


CodeGeneratorStorage::CodeGeneratorStorage(CodeEmitterBase emitter) {
	_emitter = emitter;
	_regInUse =  List<Boolean>::New();
	_firstAvailable = 0;
	_maxRegUsed = -1;
	_variableRegs =  Dictionary<String, Int32>::New();
	_targetReg = -1;
	_loopExitLabels =  List<Int32>::New();
	_loopContinueLabels =  List<Int32>::New();
	_functions =  List<FuncDef>::New();
}
List<FuncDef> CodeGeneratorStorage::GetFunctions() {
	return _functions;
}
Int32 CodeGeneratorStorage::AllocReg() {
	// Scan from _firstAvailable to find first free register
	Int32 reg = _firstAvailable;
	while (reg < _regInUse.Count() && _regInUse[reg]) {
		reg = reg + 1;
	}

	// Expand the list if needed
	while (_regInUse.Count() <= reg) {
		_regInUse.Add(Boolean(false));
	}

	// Mark register as in use
	_regInUse[reg] = Boolean(true);

	// Update _firstAvailable to search from next position
	_firstAvailable = reg + 1;

	// Update high water mark
	if (reg > _maxRegUsed) _maxRegUsed = reg;

	_emitter.ReserveRegister(reg);
	return reg;
}
void CodeGeneratorStorage::FreeReg(Int32 reg) {
	if (reg < 0 || reg >= _regInUse.Count()) return;

	_regInUse[reg] = Boolean(false);

	// Update _firstAvailable if this register is lower
	if (reg < _firstAvailable) _firstAvailable = reg;

	// Update _maxRegUsed if we freed the highest register
	if (reg == _maxRegUsed) {
		// Search downward for the new maximum register in use
		_maxRegUsed = reg - 1;
		while (_maxRegUsed >= 0 && !_regInUse[_maxRegUsed]) {
			_maxRegUsed = _maxRegUsed - 1;
		}
	}
}
Int32 CodeGeneratorStorage::AllocConsecutiveRegs(Int32 count) {
	if (count <= 0) return -1;
	if (count == 1) return AllocReg();

	// Find first position where 'count' consecutive registers are free
	Int32 startReg = _firstAvailable;
	while (Boolean(true)) {
		// Check if registers startReg through startReg+count-1 are all free
		Boolean allFree = Boolean(true);
		for (Int32 i = 0; i < count; i++) {
			Int32 reg = startReg + i;
			if (reg < _regInUse.Count() && _regInUse[reg]) {
				allFree = Boolean(false);
				startReg = reg + 1;  // Skip past this in-use register
				break;
			}
		}
		if (allFree) break;
	}

	// Allocate all registers in the block
	for (Int32 i = 0; i < count; i++) {
		Int32 reg = startReg + i;
		// Expand the list if needed
		while (_regInUse.Count() <= reg) {
			_regInUse.Add(Boolean(false));
		}
		_regInUse[reg] = Boolean(true);
		_emitter.ReserveRegister(reg);
		if (reg > _maxRegUsed) _maxRegUsed = reg;
	}

	// Update _firstAvailable
	_firstAvailable = startReg + count;

	return startReg;
}
Int32 CodeGeneratorStorage::CompileInto(ASTNode node, Int32 targetReg) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	_targetReg = targetReg;
	Int32 result = node.Accept(_this);
	_targetReg = -1;
	return result;
}
Int32 CodeGeneratorStorage::GetTargetOrAlloc() {
	Int32 target = _targetReg;
	_targetReg = -1;  // Clear immediately to avoid affecting nested calls
	if (target >= 0) return target;
	return AllocReg();
}
Int32 CodeGeneratorStorage::Compile(ASTNode ast) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	return ast.Accept(_this);
}
void CodeGeneratorStorage::ResetTempRegisters() {
	_regInUse.Clear();
	_regInUse.Add(Boolean(true));  // r0
	_firstAvailable = 1;
	for (Int32 reg : _variableRegs.GetValues()) {
		while (_regInUse.Count() <= reg) {
			_regInUse.Add(Boolean(false));
		}
		_regInUse[reg] = Boolean(true);
		if (reg >= _firstAvailable) _firstAvailable = reg + 1;
	}
}
void CodeGeneratorStorage::CompileBody(List<ASTNode> body) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	for (Int32 i = 0; i < body.Count(); i++) {
		ResetTempRegisters();
		body[i].Accept(_this);
	}
}
FuncDef CodeGeneratorStorage::CompileFunction(ASTNode ast, String funcName) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	_regInUse.Clear();
	_firstAvailable = 0;
	_maxRegUsed = -1;
	_variableRegs.Clear();

	Int32 resultReg = ast.Accept(_this);

	// Move result to r0 if not already there (and if there is a result)
	if (resultReg > 0) {
		_emitter.EmitABC(Opcode::LOAD_rA_rB, 0, resultReg, 0, "move Function result to r0");
	}
	_emitter.Emit(Opcode::RETURN, nullptr);

	return _emitter.Finalize(funcName);
}
FuncDef CodeGeneratorStorage::CompileProgram(List<ASTNode> statements, String funcName) {
	_regInUse.Clear();
	_firstAvailable = 0;
	_maxRegUsed = -1;
	_variableRegs.Clear();

	// Reserve index 0 for @main
	_functions.Clear();
	_functions.Add(nullptr);

	// Compile each statement, putting result into r0
	for (Int32 i = 0; i < statements.Count(); i++) {
		ResetTempRegisters();
		CompileInto(statements[i], 0);
	}

	_emitter.Emit(Opcode::RETURN, nullptr);

	FuncDef mainFunc = _emitter.Finalize(funcName);
	_functions[0] = mainFunc;
	return mainFunc;
}
Int32 CodeGeneratorStorage::Visit(NumberNode node) {
	Int32 reg = GetTargetOrAlloc();
	Double value = node.Value();

	// Check if value fits in signed 16-bit immediate
	if (value == Math::Floor(value) && value >= -32768 && value <= 32767) {
		_emitter.EmitAB(Opcode::LOAD_rA_iBC, reg, (Int32)value, Interp("r{} = {}", reg, value));
	} else {
		// Store in constants and load from there
		Int32 constIdx = _emitter.AddConstant(make_double(value));
		_emitter.EmitAB(Opcode::LOAD_rA_kBC, reg, constIdx, Interp("r{} = {}", reg, value));
	}
	return reg;
}
Int32 CodeGeneratorStorage::Visit(StringNode node) {
	Int32 reg = GetTargetOrAlloc();
	Int32 constIdx = _emitter.AddConstant(make_string(node.Value()));
	_emitter.EmitAB(Opcode::LOAD_rA_kBC, reg, constIdx, Interp("r{} = \"{}\"", reg, node.Value()));
	return reg;
}
Int32 CodeGeneratorStorage::VisitIdentifier(IdentifierNode node, bool addressOf) {
	Int32 resultReg = GetTargetOrAlloc();

	Int32 varReg;
	if (_variableRegs.TryGetValue(node.Name(), &varReg)) {
		// Variable found - emit LOADC (load-and-call for implicit function invocation)
		Int32 nameIdx = _emitter.AddConstant(make_string(node.Name()));
		Opcode op = addressOf ? Opcode::LOADV_rA_rB_kC : Opcode::LOADC_rA_rB_kC;
		String a = addressOf ? "@" : "";
		_emitter.EmitABC(op, resultReg, varReg, nameIdx,
			Interp("r{} = {}{}", resultReg, a, node.Name()));
	} else {
		// Undefined variable - for now just load 0
		// TODO: handle outer/globals lookup or report error
		_emitter.EmitAB(Opcode::LOAD_rA_iBC, resultReg, 0, Interp("undefined: {}", node.Name()));
	}

	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(IdentifierNode node) {
	return VisitIdentifier(node, Boolean(false));
}
Int32 CodeGeneratorStorage::Visit(AssignmentNode node) {
	// Note the desired target register, if any (but see notes below).
	Int32 explicitTarget = _targetReg;
	if (_targetReg > 0) {
		Errors.Add(StringUtils::Format("Compiler Error: unexpected target register {0} in assignment", _targetReg));
	}
	
	// Get or allocate register for this variable
	Int32 varReg;
	if (_variableRegs.TryGetValue(node.Variable(), &varReg)) {
		// Variable already has a register - reuse it (and don't bother with naming)
	} else {
		// Hmm.  Should we allocate a new register for this variable, or
		// just claim the target register as our storage?  I'm going to alloc
		// a new one for now, because I can't be sure the caller won't free
		// the target register when done.  But we should probably return to
		// this later and see if we can optimize it more.
		varReg = AllocReg();
		_variableRegs[node.Variable()] = varReg;
		Int32 nameIdx = _emitter.AddConstant(make_string(node.Variable()));
		_emitter.EmitAB(Opcode::NAME_rA_kBC, varReg, nameIdx, Interp("use r{} for {}", varReg, node.Variable()));
	}
	CompileInto(node.Value(), varReg);  // get RHS directly into the variable's register

	// Note that we don't FreeReg(varReg) here, as we need this register to
	// continue to serve as the storage for this variable for the life of
	// the function.  (TODO: or until some lifetime analysis determines that
	// the variable will never be read again.)

	// BUT, if varReg is not the explicit target register, then we need to copy
	// the value there as well.  Not sure why that would ever be the case (since
	// assignment can't be used in an expression in MiniScript).  So:
	return varReg;
}
Int32 CodeGeneratorStorage::Visit(IndexedAssignmentNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 containerReg = node.Target().Accept(_this);
	Int32 indexReg = node.Index().Accept(_this);
	Int32 valueReg = node.Value().Accept(_this);

	_emitter.EmitABC(Opcode::IDXSET_rA_rB_rC, containerReg, indexReg, valueReg,
		Interp("{}[{}] = {}", node.Target().ToStr(), node.Index().ToStr(), node.Value().ToStr()));

	FreeReg(valueReg);
	FreeReg(indexReg);
	return containerReg;
}
Int32 CodeGeneratorStorage::Visit(UnaryOpNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	if (node.Op() == Op::ADDRESS_OF) {
		// Special case: just an identifier lookup without function call
		IdentifierNode ident = As<IdentifierNode, IdentifierNodeStorage>(node.Operand());
		if (!IsNull(ident)) return VisitIdentifier(ident, Boolean(true));
		// On anything other than an identifier, @ has no effect.
		return node.Operand().Accept(_this);
	}
		
	Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls

	Int32 operandReg = node.Operand().Accept(_this);

	if (node.Op() == Op::MINUS) {
		// Negate: result = 0 - operand
		Int32 zeroReg = AllocReg();
		_emitter.EmitAB(Opcode::LOAD_rA_iBC, zeroReg, 0, "r{zeroReg} = 0 (for negation)");
		_emitter.EmitABC(Opcode::SUB_rA_rB_rC, resultReg, zeroReg, operandReg, Interp("r{} = -{}", resultReg, node.Operand().ToStr()));
		FreeReg(zeroReg);
		FreeReg(operandReg);
		return resultReg;
	} else if (node.Op() == Op::NOT) {
		// Fuzzy logic NOT: 1 - AbsClamp01(operand)
		_emitter.EmitABC(Opcode::NOT_rA_rB, resultReg, operandReg, 0, Interp("not {}", node.Operand().ToStr()));
		FreeReg(operandReg);
		return resultReg;
	}

	// Unknown unary operator - move operand to result if needed
	Errors.Add("Compiler Error: unknown unary operator");
	if (operandReg != resultReg) {
		_emitter.EmitABC(Opcode::LOAD_rA_rB, resultReg, operandReg, 0, "move to target");
		FreeReg(operandReg);
	}
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(BinaryOpNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls
	Int32 leftReg = node.Left().Accept(_this);
	Int32 rightReg = node.Right().Accept(_this);

	Opcode op = Opcode::NOOP;
	String opSymbol = "?";

	if (node.Op() == Op::PLUS) {
		op = Opcode::ADD_rA_rB_rC;
		opSymbol = "+";
	} else if (node.Op() == Op::MINUS) {
		op = Opcode::SUB_rA_rB_rC;
		opSymbol = "-";
	} else if (node.Op() == Op::TIMES) {
		op = Opcode::MULT_rA_rB_rC;
		opSymbol = "*";
	} else if (node.Op() == Op::DIVIDE) {
		op = Opcode::DIV_rA_rB_rC;
		opSymbol = "/";
	} else if (node.Op() == Op::MOD) {
		op = Opcode::MOD_rA_rB_rC;
		opSymbol = "%";
	} else if (node.Op() == Op::LESS_THAN) {
		op = Opcode::LT_rA_rB_rC;
		opSymbol = "<";
	} else if (node.Op() == Op::LESS_EQUAL) {
		op = Opcode::LE_rA_rB_rC;
		opSymbol = "<=";
	} else if (node.Op() == Op::EQUALS) {
		op = Opcode::EQ_rA_rB_rC;
		opSymbol = "==";
	} else if (node.Op() == Op::NOT_EQUAL) {
		op = Opcode::NE_rA_rB_rC;
		opSymbol = "!=";
	} else if (node.Op() == Op::GREATER_THAN) {
		// a > b is same as b < a
		op = Opcode::LT_rA_rB_rC;
		opSymbol = ">";
		// Swap operands
		Int32 temp = leftReg;
		leftReg = rightReg;
		rightReg = temp;
	} else if (node.Op() == Op::GREATER_EQUAL) {
		// a >= b is same as b <= a
		op = Opcode::LE_rA_rB_rC;
		opSymbol = ">=";
		// Swap operands
		Int32 temp = leftReg;
		leftReg = rightReg;
		rightReg = temp;
	} else if (node.Op() == Op::POWER) {
		// Power requires a function call - not yet implemented
		// For now, emit a placeholder
		op = Opcode::NOOP;
		opSymbol = "^";
	} else if (node.Op() == Op::AND) {
		// Fuzzy logic AND: AbsClamp01(a * b)
		op = Opcode::AND_rA_rB_rC;
		opSymbol = "and";
	} else if (node.Op() == Op::OR) {
		// Fuzzy logic OR: AbsClamp01(a + b - a*b)
		op = Opcode::OR_rA_rB_rC;
		opSymbol = "or";
	}

	if (op != Opcode::NOOP) {
		_emitter.EmitABC(op, resultReg, leftReg, rightReg,
			Interp("r{} = {} {} {}", resultReg, node.Left().ToStr(), opSymbol, node.Right().ToStr()));
	}

	FreeReg(rightReg);
	FreeReg(leftReg);
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(CallNode node) {
	// Capture target register if one was specified (don't allocate yet)
	Int32 explicitTarget = _targetReg;
	_targetReg = -1;

	Int32 argCount = node.Arguments().Count();

	// Check if the function is a known variable (user-defined function)
	Int32 funcVarReg;
	if (_variableRegs.TryGetValue(node.Function(), &funcVarReg)) {
		// User-defined function call: ARGBLK + ARGs + CALL_rA_rB_rC
		return CompileUserCall(node, funcVarReg, explicitTarget);
	}

	// Intrinsic function call: CALLFN
	// Arguments loaded into consecutive registers starting at baseReg
	// Return value placed in baseReg after the call

	// Allocate consecutive registers for arguments
	// The first register (baseReg) will also hold the return value
	Int32 baseReg = (argCount > 0) ? AllocConsecutiveRegs(argCount) : AllocReg();

	// Compile each argument directly into its target register
	for (Int32 i = 0; i < argCount; i++) {
		CompileInto(node.Arguments()[i], baseReg + i);
	}

	// Emit the function call
	// CALLFN_iA_kBC: A = base register, BC = constant index for function name
	Int32 funcNameIdx = _emitter.AddConstant(make_string(node.Function()));
	_emitter.EmitAB(Opcode::CALLFN_iA_kBC, baseReg, funcNameIdx, Interp("call {}", node.Function()));

	// Free any extra argument registers (keeping baseReg for return value)
	for (Int32 i = 1; i < argCount; i++) {
		FreeReg(baseReg + i);
	}

	// If an explicit target was requested and it's different, move result there
	if (explicitTarget >= 0 && explicitTarget != baseReg) {
		_emitter.EmitABC(Opcode::LOAD_rA_rB, explicitTarget, baseReg, 0, Interp("move Call result to r{}", explicitTarget));
		FreeReg(baseReg);
		return explicitTarget;
	}

	return baseReg;
}
Int32 CodeGeneratorStorage::CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 argCount = node.Arguments().Count();

	// Compile arguments into temporary registers
	List<Int32> argRegs =  List<Int32>::New();
	for (Int32 i = 0; i < argCount; i++) {
		argRegs.Add(node.Arguments()[i].Accept(_this));
	}

	// Emit ARGBLK (24-bit arg count)
	_emitter.EmitABC(Opcode::ARGBLK_iABC, 0, 0, argCount, Interp("argblock {}", argCount));

	// Emit ARG for each argument
	for (Int32 i = 0; i < argCount; i++) {
		_emitter.EmitA(Opcode::ARG_rA, argRegs[i], Interp("arg {}", i));
	}

	// Determine base register for callee frame (past all our used registers)
	Int32 calleeBase = _maxRegUsed + 1;
	_emitter.ReserveRegister(calleeBase);

	// Determine result register
	Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

	// Emit CALL: result in rA, callee frame at rB, funcref in rC
	_emitter.EmitABC(Opcode::CALL_rA_rB_rC, resultReg, calleeBase, funcVarReg,
		Interp("call {}, result to r{}", node.Function(), resultReg));

	// Free argument registers
	for (Int32 i = 0; i < argCount; i++) {
		FreeReg(argRegs[i]);
	}

	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(GroupNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Groups just delegate to their inner expression
	return node.Expression().Accept(_this);
}
Int32 CodeGeneratorStorage::Visit(ListNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Create a list with the given number of elements
	Int32 listReg = GetTargetOrAlloc();
	Int32 count = node.Elements().Count();
	_emitter.EmitAB(Opcode::LIST_rA_iBC, listReg, count, Interp("r{} = new list[{}]", listReg, count));

	// Push each element onto the list
	for (Int32 i = 0; i < count; i++) {
		Int32 elemReg = node.Elements()[i].Accept(_this);
		_emitter.EmitABC(Opcode::PUSH_rA_rB, listReg, elemReg, 0, Interp("push element {} onto r{}", i, listReg));
		FreeReg(elemReg);
	}

	return listReg;
}
Int32 CodeGeneratorStorage::Visit(MapNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Create a map
	Int32 mapReg = GetTargetOrAlloc();
	Int32 count = node.Keys().Count();
	_emitter.EmitAB(Opcode::MAP_rA_iBC, mapReg, count, Interp("new map[{}]", count));

	// Set each key-value pair
	for (Int32 i = 0; i < count; i++) {
		Int32 keyReg = node.Keys()[i].Accept(_this);
		Int32 valueReg = node.Values()[i].Accept(_this);
		_emitter.EmitABC(Opcode::IDXSET_rA_rB_rC, mapReg, keyReg, valueReg, Interp("map[{}] = {}", node.Keys()[i].ToStr(), node.Values()[i].ToStr()));
		FreeReg(valueReg);
		FreeReg(keyReg);
	}

	return mapReg;
}
Int32 CodeGeneratorStorage::Visit(IndexNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls
	Int32 targetReg = node.Target().Accept(_this);
	Int32 indexReg = node.Index().Accept(_this);

	_emitter.EmitABC(Opcode::INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
		Interp("{}[{}]", node.Target().ToStr(), node.Index().ToStr()));

	FreeReg(indexReg);
	FreeReg(targetReg);
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(MemberNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Member access not yet fully implemented
	Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls
	Int32 targetReg = node.Target().Accept(_this);
	// TODO: Implement member access
	_emitter.EmitABC(Opcode::LOAD_rA_rB, resultReg, targetReg, 0, Interp("TODO: {}.{}", node.Target().ToStr(), node.Member()));
	FreeReg(targetReg);
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(ExprCallNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Call a function referenced by an arbitrary expression (e.g. funcs[0](10))
	Int32 explicitTarget = _targetReg;
	_targetReg = -1;

	Int32 argCount = node.Arguments().Count();

	// Evaluate the function expression to get the funcref
	Int32 funcReg = node.Function().Accept(_this);

	// Compile arguments into temporary registers
	List<Int32> argRegs =  List<Int32>::New();
	for (Int32 i = 0; i < argCount; i++) {
		argRegs.Add(node.Arguments()[i].Accept(_this));
	}

	// Emit ARGBLK (24-bit arg count)
	_emitter.EmitABC(Opcode::ARGBLK_iABC, 0, 0, argCount, Interp("argblock {}", argCount));

	// Emit ARG for each argument
	for (Int32 i = 0; i < argCount; i++) {
		_emitter.EmitA(Opcode::ARG_rA, argRegs[i], Interp("arg {}", i));
	}

	// Determine base register for callee frame (past all our used registers)
	Int32 calleeBase = _maxRegUsed + 1;
	_emitter.ReserveRegister(calleeBase);

	// Determine result register
	Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

	// Emit CALL: result in rA, callee frame at rB, funcref in rC
	_emitter.EmitABC(Opcode::CALL_rA_rB_rC, resultReg, calleeBase, funcReg,
		Interp("call expr, result to r{}", resultReg));

	// Free argument registers and funcref register
	for (Int32 i = 0; i < argCount; i++) {
		FreeReg(argRegs[i]);
	}
	FreeReg(funcReg);

	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(MethodCallNode node) {
	// Method calls not yet implemented
	Int32 reg = GetTargetOrAlloc();
	_emitter.EmitAB(Opcode::LOAD_rA_iBC, reg, 0, Interp("TODO: {}.{}()", node.Target().ToStr(), node.Method()));
	return reg;
}
Int32 CodeGeneratorStorage::Visit(WhileNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// While loop generates:
	//   loopStart:
	//       [evaluate condition]
	//       BRFALSE condReg, afterLoop
	//       [body statements]
	//       JUMP loopStart
	//   afterLoop:

	Int32 loopStart = _emitter.CreateLabel();
	Int32 afterLoop = _emitter.CreateLabel();

	// Push labels for break and continue statements
	_loopExitLabels.Add(afterLoop);
	_loopContinueLabels.Add(loopStart);

	// Place loopStart label
	_emitter.PlaceLabel(loopStart);

	// Evaluate condition
	Int32 condReg = node.Condition().Accept(_this);

	// Branch to afterLoop if condition is false
	_emitter.EmitBranch(Opcode::BRFALSE_rA_iBC, condReg, afterLoop, "exit loop if false");
	FreeReg(condReg);

	// Compile body statements
	CompileBody(node.Body());

	// Jump back to loopStart
	_emitter.EmitJump(Opcode::JUMP_iABC, loopStart, "loop back");

	// Place afterLoop label
	_emitter.PlaceLabel(afterLoop);

	// Pop labels
	_loopExitLabels.RemoveAt(_loopExitLabels.Count() - 1);
	_loopContinueLabels.RemoveAt(_loopContinueLabels.Count() - 1);

	// While loops don't produce a value
	return -1;
}
Int32 CodeGeneratorStorage::Visit(IfNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// If statement generates:
	//       [evaluate condition]
	//       BRFALSE condReg, elseLabel (or afterIf if no else)
	//       [then body]
	//       JUMP afterIf
	//   elseLabel:
	//       [else body]
	//   afterIf:

	Int32 afterIf = _emitter.CreateLabel();
	Int32 elseLabel = (node.ElseBody().Count() > 0) ? _emitter.CreateLabel() : afterIf;

	// Evaluate condition
	Int32 condReg = node.Condition().Accept(_this);

	// Branch to else (or afterIf) if condition is false
	_emitter.EmitBranch(Opcode::BRFALSE_rA_iBC, condReg, elseLabel, "if condition false, jump to else");
	FreeReg(condReg);

	// Compile "then" body
	CompileBody(node.ThenBody());

	// Jump over else body (if there is one)
	if (node.ElseBody().Count() > 0) {
		_emitter.EmitJump(Opcode::JUMP_iABC, afterIf, "jump past else");

		// Place else label
		_emitter.PlaceLabel(elseLabel);

		// Compile "else" body
		CompileBody(node.ElseBody());
	}

	// Place afterIf label
	_emitter.PlaceLabel(afterIf);

	// If statements don't produce a value
	return -1;
}
Int32 CodeGeneratorStorage::Visit(ForNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// For loop generates (using NEXT opcode for performance):
	//   [evaluate iterable into listReg]
	//   indexReg = -1  (NEXT increments before checking)
	// loopStart:
	//   NEXT indexReg, listReg  (indexReg++; if indexReg >= len(listReg) skip next)
	//   JUMP afterLoop          (skipped unless we've reached the end)
	//   varReg = listReg[indexReg]
	//   [body statements]
	//   JUMP loopStart
	// afterLoop:

	Int32 loopStart = _emitter.CreateLabel();
	Int32 afterLoop = _emitter.CreateLabel();

	// Push labels for break and continue statements
	_loopExitLabels.Add(afterLoop);
	_loopContinueLabels.Add(loopStart);

	// Evaluate iterable expression
	Int32 listReg = node.Iterable().Accept(_this);

	// Allocate hidden index register (starts at -1; NEXT will increment to 0)
	Int32 indexReg = AllocReg();
	_emitter.EmitAB(Opcode::LOAD_rA_iBC, indexReg, -1, "for loop index = -1");

	// Get or create register for loop variable
	Int32 varReg;
	if (_variableRegs.TryGetValue(node.Variable(), &varReg)) {
		// Variable already exists
	} else {
		varReg = AllocReg();
		_variableRegs[node.Variable()] = varReg;
		Int32 nameIdx = _emitter.AddConstant(make_string(node.Variable()));
		_emitter.EmitAB(Opcode::NAME_rA_kBC, varReg, nameIdx, Interp("use r{} for {}", varReg, node.Variable()));
	}

	// Place loopStart label
	_emitter.PlaceLabel(loopStart);

	// NEXT: increment index, skip next if done
	_emitter.EmitABC(Opcode::NEXT_rA_rB, indexReg, listReg, 0, "index++; skip next if done");
	_emitter.EmitJump(Opcode::JUMP_iABC, afterLoop, "exit loop");

	// Get current element: varReg = listReg[indexReg]
	_emitter.EmitABC(Opcode::INDEX_rA_rB_rC, varReg, listReg, indexReg, Interp("{} = list[index]", node.Variable()));

	// Compile body statements
	CompileBody(node.Body());

	// Jump back to loopStart
	_emitter.EmitJump(Opcode::JUMP_iABC, loopStart, "loop back");

	// Place afterLoop label
	_emitter.PlaceLabel(afterLoop);

	// Pop labels
	_loopExitLabels.RemoveAt(_loopExitLabels.Count() - 1);
	_loopContinueLabels.RemoveAt(_loopContinueLabels.Count() - 1);

	// Free temporary registers (but keep variable register)
	FreeReg(indexReg);
	FreeReg(listReg);

	// For loops don't produce a value
	return -1;
}
Int32 CodeGeneratorStorage::Visit(BreakNode node) {
	// Break jumps to the innermost loop's exit label
	if (_loopExitLabels.Count() == 0) {
		Errors.Add("Compiler Error: 'break' without open loop block");
		_emitter.Emit(Opcode::NOOP, "break outside loop (error)");
	} else {
		Int32 exitLabel = _loopExitLabels[_loopExitLabels.Count() - 1];
		_emitter.EmitJump(Opcode::JUMP_iABC, exitLabel, "break");
	}
	return -1;
}
Int32 CodeGeneratorStorage::Visit(ContinueNode node) {
	// Continue jumps to the innermost loop's continue label (loop start)
	if (_loopContinueLabels.Count() == 0) {
		Errors.Add("Compiler Error: 'continue' without open loop block");
		_emitter.Emit(Opcode::NOOP, "continue outside loop (error)");
	} else {
		Int32 continueLabel = _loopContinueLabels[_loopContinueLabels.Count() - 1];
		_emitter.EmitJump(Opcode::JUMP_iABC, continueLabel, "continue");
	}
	return -1;
}
Boolean CodeGeneratorStorage::TryEvaluateConstant(ASTNode node, Value* result) {
	*result = make_null();
	NumberNode numNode = As<NumberNode, NumberNodeStorage>(node);
	if (!IsNull(numNode)) {
		*result = make_double(numNode.Value());
		return Boolean(true);
	}
	StringNode strNode = As<StringNode, StringNodeStorage>(node);
	if (!IsNull(strNode)) {
		*result = make_string(strNode.Value());
		return Boolean(true);
	}
	IdentifierNode idNode = As<IdentifierNode, IdentifierNodeStorage>(node);
	if (!IsNull(idNode)) {
		if (idNode.Name() == "null") { *result = make_null(); return Boolean(true); }
		if (idNode.Name() == "true") { *result = make_double(1); return Boolean(true); }
		if (idNode.Name() == "false") { *result = make_double(0); return Boolean(true); }
		return Boolean(false);
	}
	UnaryOpNode unaryNode = As<UnaryOpNode, UnaryOpNodeStorage>(node);
	if (!IsNull(unaryNode) && unaryNode.Op() == Op::MINUS) {
		NumberNode innerNum = As<NumberNode, NumberNodeStorage>(unaryNode.Operand());
		if (!IsNull(innerNum)) {
			*result = make_double(-innerNum.Value());
			return Boolean(true);
		}
		return Boolean(false);
	}
	GC_PUSH_SCOPE();
	Value list; GC_PROTECT(&list);
	Value elemVal; GC_PROTECT(&elemVal);
	ListNode listNode = As<ListNode, ListNodeStorage>(node);
	if (!IsNull(listNode)) {
		list = make_list(listNode.Elements().Count());
		for (Int32 i = 0; i < listNode.Elements().Count(); i++) {
			if (!TryEvaluateConstant(listNode.Elements()[i], &elemVal))  {
				GC_POP_SCOPE();
				return Boolean(false);
			}
			list_push(list, elemVal);
		}
		freeze_value(list);
		*result = list;
		GC_POP_SCOPE();
		return Boolean(true);
	}
	Value map; GC_PROTECT(&map);
	Value keyVal; GC_PROTECT(&keyVal);
	Value valVal; GC_PROTECT(&valVal);
	MapNode mapNode = As<MapNode, MapNodeStorage>(node);
	if (!IsNull(mapNode)) {
		map = make_map(mapNode.Keys().Count());
		for (Int32 i = 0; i < mapNode.Keys().Count(); i++) {
			if (!TryEvaluateConstant(mapNode.Keys()[i], &keyVal))  {
				GC_POP_SCOPE();
				return Boolean(false);
			}
			if (!TryEvaluateConstant(mapNode.Values()[i], &valVal))  {
				GC_POP_SCOPE();
				return Boolean(false);
			}
			map_set(map, keyVal, valVal);
		}
		freeze_value(map);
		*result = map;
		GC_POP_SCOPE();
		return Boolean(true);
	}
	GC_POP_SCOPE();
	return Boolean(false);
}
Int32 CodeGeneratorStorage::Visit(FunctionNode node) {
	Int32 resultReg = GetTargetOrAlloc();

	// Reserve an index for this function in the shared list
	Int32 funcIndex = _functions.Count();
	_functions.Add(nullptr);  // placeholder

	// Create a new CodeGenerator for the inner function
	BytecodeEmitter innerEmitter =  BytecodeEmitter::New();
	CodeGenerator innerGen =  CodeGenerator::New(innerEmitter);
	innerGen.set__functions(_functions);  // share the function list
	innerGen.set_Errors(Errors);

	// Reserve r0 for return value, then set up param registers (r1, r2, ...)
	innerGen.AllocReg();  // r0 reserved for return value
	for (Int32 i = 0; i < node.ParamNames().Count(); i++) {
		Int32 paramReg = innerGen.AllocReg();  // r1, r2, ...
		String name = node.ParamNames()[i];
		innerGen._variableRegs()[name] = paramReg;
		Int32 nameIdx = innerEmitter.AddConstant(make_string(name));
		innerEmitter.EmitAB(Opcode::NAME_rA_kBC, paramReg, nameIdx, Interp("param {}", name));
	}

	// Compile the function body
	innerGen.CompileBody(node.Body());

	// Emit implicit RETURN at end of body
	innerEmitter.Emit(Opcode::RETURN, nullptr);

	// Finalize the inner function
	String funcName = StringUtils::Format("@f{0}", funcIndex);
	FuncDef funcDef = innerEmitter.Finalize(funcName);

	// Set parameter info on the FuncDef
	GC_PUSH_SCOPE();
	Value defaultVal; GC_PROTECT(&defaultVal);
	for (Int32 i = 0; i < node.ParamNames().Count(); i++) {
		funcDef.ParamNames().Add(make_string(node.ParamNames()[i]));
		ASTNode defaultNode = node.ParamDefaults()[i];
		if (!IsNull(defaultNode)) {
			if (TryEvaluateConstant(defaultNode, &defaultVal)) {
				funcDef.ParamDefaults().Add(defaultVal);
			} else {
				Errors.Add(StringUtils::Format("Default value for parameter '{0}' must be a constant", node.ParamNames()[i]));
				funcDef.ParamDefaults().Add(make_null());
			}
		} else {
			funcDef.ParamDefaults().Add(make_null());
		}
	}

	// Store in the shared functions list
	_functions[funcIndex] = funcDef;

	// In the outer function, emit FUNCREF to create a reference
	_emitter.EmitAB(Opcode::FUNCREF_iA_iBC, resultReg, funcIndex,
		Interp("r{} = funcref {}", resultReg, funcName));

	GC_POP_SCOPE();
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(ReturnNode node) {
	// Compile return value into r0, then emit RETURN
	if (!IsNull(node.Value())) {
		CompileInto(node.Value(), 0);
	}
	_emitter.Emit(Opcode::RETURN, nullptr);
	return -1;
}


} // end of namespace MiniScript
