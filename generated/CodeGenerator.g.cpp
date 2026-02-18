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

	// Handle built-in constants
	if (node.Name() == "null") {
		_emitter.EmitA(Opcode::LOADNULL_rA, resultReg, Interp("r{} = null", resultReg));
		return resultReg;
	}
	if (node.Name() == "true") {
		_emitter.EmitAB(Opcode::LOAD_rA_iBC, resultReg, 1, Interp("r{} = true", resultReg));
		return resultReg;
	}
	if (node.Name() == "false") {
		_emitter.EmitAB(Opcode::LOAD_rA_iBC, resultReg, 0, Interp("r{} = false", resultReg));
		return resultReg;
	}

	Int32 varReg;
	if (_variableRegs.TryGetValue(node.Name(), &varReg)) {
		// Variable found - emit LOADC (load-and-call for implicit function invocation)
		Int32 nameIdx = _emitter.AddConstant(make_string(node.Name()));
		Opcode op = addressOf ? Opcode::LOADV_rA_rB_kC : Opcode::LOADC_rA_rB_kC;
		String a = addressOf ? "@" : "";
		_emitter.EmitABC(op, resultReg, varReg, nameIdx,
			Interp("r{} = {}{}", resultReg, a, node.Name()));
	} else {
		// Variable not in local scope — emit LOADC/LOADV referencing r0 with
		// the variable name. At runtime, the name check on r0 will fail,
		// triggering LookupVariable to search outer/global scope.
		Int32 nameIdx = _emitter.AddConstant(make_string(node.Name()));
		Opcode op = addressOf ? Opcode::LOADV_rA_rB_kC : Opcode::LOADC_rA_rB_kC;
		String a = addressOf ? "@" : "";
		_emitter.EmitABC(op, resultReg, 0, nameIdx,
			Interp("r{} = {}{} (outer)", resultReg, a, node.Name()));
	}

	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(IdentifierNode node) {
	return VisitIdentifier(node, Boolean(false));
}
Int32 CodeGeneratorStorage::Visit(AssignmentNode node) {
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
		// Special case: lookup without function call (address-of)
		IdentifierNode ident = As<IdentifierNode, IdentifierNodeStorage>(node.Operand());
		if (!IsNull(ident)) return VisitIdentifier(ident, Boolean(true));
		MemberNode member = As<MemberNode, MemberNodeStorage>(node.Operand());
		if (!IsNull(member)) return VisitMember(member, Boolean(true));
		IndexNode index = As<IndexNode, IndexNodeStorage>(node.Operand());
		if (!IsNull(index)) return VisitIndex(index, Boolean(true));
		// On anything else, @ has no effect.
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
	} else if (node.Op() == Op::NEW) {
		// new: create a map with __isa set to the operand
		_emitter.EmitABC(Opcode::NEW_rA_rB, resultReg, operandReg, 0, Interp("new {}", node.Operand().ToStr()));
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
	} else if (node.Op() == Op::ISA) {
		op = Opcode::ISA_rA_rB_rC;
		opSymbol = "isa";
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
	return VisitIndex(node, Boolean(false));
}
Int32 CodeGeneratorStorage::VisitIndex(IndexNode node, bool addressOf) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls
	Int32 targetReg = node.Target().Accept(_this);
	Int32 indexReg = node.Index().Accept(_this);

	if (addressOf) {
		_emitter.EmitABC(Opcode::INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
			Interp("@{}[{}]", node.Target().ToStr(), node.Index().ToStr()));
	} else {
		_emitter.EmitABC(Opcode::METHFIND_rA_rB_rC, resultReg, targetReg, indexReg,
			Interp("{}[{}]", node.Target().ToStr(), node.Index().ToStr()));
		SuperNode superTarget = As<SuperNode, SuperNodeStorage>(node.Target());
		if (!IsNull(superTarget)) {
			Int32 selfReg = GetSelfReg();
			_emitter.EmitA(Opcode::SETSELF_rA, selfReg, Interp("preserve self for super access"));
		}
		_emitter.EmitA(Opcode::CALLIFREF_rA, resultReg, Interp("auto-invoke if funcref"));
	}

	FreeReg(indexReg);
	FreeReg(targetReg);
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(SliceNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 resultReg = GetTargetOrAlloc();
	Int32 containerReg = node.Target().Accept(_this);

	// Allocate two consecutive registers for start and end indices
	Int32 startReg = AllocConsecutiveRegs(2);
	Int32 endReg = startReg + 1;

	if (!IsNull(node.StartIndex())) {
		CompileInto(node.StartIndex(), startReg);
	} else {
		_emitter.EmitA(Opcode::LOADNULL_rA, startReg, Interp("r{} = null (slice start)", startReg));
	}

	if (!IsNull(node.EndIndex())) {
		CompileInto(node.EndIndex(), endReg);
	} else {
		_emitter.EmitA(Opcode::LOADNULL_rA, endReg, Interp("r{} = null (slice end)", endReg));
	}

	_emitter.EmitABC(Opcode::SLICE_rA_rB_rC, resultReg, containerReg, startReg,
		Interp("r{} = {}[{}]", resultReg, node.Target().ToStr(), node.ToStr()));

	FreeReg(endReg);
	FreeReg(startReg);
	FreeReg(containerReg);
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(MemberNode node) {
	return VisitMember(node, Boolean(false));
}
Int32 CodeGeneratorStorage::VisitMember(MemberNode node, bool addressOf) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 resultReg = GetTargetOrAlloc();
	Int32 targetReg = node.Target().Accept(_this);
	Int32 indexReg = AllocReg();
	Int32 constIdx = _emitter.AddConstant(make_string(node.Member()));
	_emitter.EmitAB(Opcode::LOAD_rA_kBC, indexReg, constIdx, Interp("r{} = \"{}\"", indexReg, node.Member()));

	if (addressOf) {
		// @ prefix: plain INDEX, no auto-invoke
		_emitter.EmitABC(Opcode::INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
			Interp("@{}.{}", node.Target().ToStr(), node.Member()));
	} else {
		// Normal access: METHFIND + optional SETSELF + CALLIFREF
		_emitter.EmitABC(Opcode::METHFIND_rA_rB_rC, resultReg, targetReg, indexReg,
			Interp("{}.{}", node.Target().ToStr(), node.Member()));
		// If target is 'super', preserve current self
		SuperNode superTarget = As<SuperNode, SuperNodeStorage>(node.Target());
		if (!IsNull(superTarget)) {
			Int32 selfReg = GetSelfReg();
			_emitter.EmitA(Opcode::SETSELF_rA, selfReg, Interp("preserve self for super access"));
		}
		_emitter.EmitA(Opcode::CALLIFREF_rA, resultReg, Interp("auto-invoke if funcref"));
	}

	FreeReg(indexReg);
	FreeReg(targetReg);
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(ExprCallNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Check if the function expression is a member access or index operation
	// on a map — if so, this is a method call and we need to set self/super.
	MemberNode memberTarget = As<MemberNode, MemberNodeStorage>(node.Function());
	if (!IsNull(memberTarget)) {
		// obj.method() via ExprCallNode — treat as method call
		SuperNode superTarget = As<SuperNode, SuperNodeStorage>(memberTarget.Target());
		bool preserveSelf = (!IsNull(superTarget));
		Int32 receiverReg = memberTarget.Target().Accept(_this);
		Int32 resultReg = EmitMethodCall(receiverReg, memberTarget.Member(), node.Arguments(), preserveSelf);
		FreeReg(receiverReg);
		return resultReg;
	}

	IndexNode indexTarget = As<IndexNode, IndexNodeStorage>(node.Function());
	if (!IsNull(indexTarget)) {
		// obj[key]() — treat as method call if key is a string
		SuperNode superTarget = As<SuperNode, SuperNodeStorage>(indexTarget.Target());
		bool preserveSelf = (!IsNull(superTarget));
		Int32 explicitTarget = _targetReg;
		_targetReg = -1;

		// Evaluate receiver and key
		Int32 receiverReg = indexTarget.Target().Accept(_this);
		Int32 keyReg = indexTarget.Index().Accept(_this);

		// Use METHFIND instead of INDEX
		Int32 funcReg = AllocReg();
		_emitter.EmitABC(Opcode::METHFIND_rA_rB_rC, funcReg, receiverReg, keyReg,
			Interp("r{} = method lookup", funcReg));

		// For super[key]() calls, preserve self
		if (preserveSelf) {
			Int32 selfReg = GetSelfReg();
			_emitter.EmitA(Opcode::SETSELF_rA, selfReg, Interp("preserve self for super call"));
		}

		FreeReg(keyReg);

		// Compile arguments
		Int32 argCount = node.Arguments().Count();
		List<Int32> argRegs =  List<Int32>::New();
		for (Int32 i = 0; i < argCount; i++) {
			argRegs.Add(node.Arguments()[i].Accept(_this));
		}

		_emitter.EmitABC(Opcode::ARGBLK_iABC, 0, 0, argCount, Interp("argblock {}", argCount));
		for (Int32 i = 0; i < argCount; i++) {
			_emitter.EmitA(Opcode::ARG_rA, argRegs[i], Interp("arg {}", i));
		}

		Int32 calleeBase = _maxRegUsed + 1;
		_emitter.ReserveRegister(calleeBase);
		Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

		_emitter.EmitABC(Opcode::CALL_rA_rB_rC, resultReg, calleeBase, funcReg,
			Interp("method call via index, result to r{}", resultReg));

		for (Int32 i = 0; i < argCount; i++) {
			FreeReg(argRegs[i]);
		}
		FreeReg(funcReg);
		FreeReg(receiverReg);
		return resultReg;
	}

	// Regular function call (not a method call)
	Int32 explicitTarget2 = _targetReg;
	_targetReg = -1;

	Int32 argCount2 = node.Arguments().Count();

	// Evaluate the function expression to get the funcref
	Int32 funcReg2 = node.Function().Accept(_this);

	// Compile arguments into temporary registers
	List<Int32> argRegs2 =  List<Int32>::New();
	for (Int32 i = 0; i < argCount2; i++) {
		argRegs2.Add(node.Arguments()[i].Accept(_this));
	}

	// Emit ARGBLK (24-bit arg count)
	_emitter.EmitABC(Opcode::ARGBLK_iABC, 0, 0, argCount2, Interp("argblock {}", argCount2));

	// Emit ARG for each argument
	for (Int32 i = 0; i < argCount2; i++) {
		_emitter.EmitA(Opcode::ARG_rA, argRegs2[i], Interp("arg {}", i));
	}

	// Determine base register for callee frame (past all our used registers)
	Int32 calleeBase2 = _maxRegUsed + 1;
	_emitter.ReserveRegister(calleeBase2);

	// Determine result register
	Int32 resultReg2 = (explicitTarget2 >= 0) ? explicitTarget2 : AllocReg();

	// Emit CALL: result in rA, callee frame at rB, funcref in rC
	_emitter.EmitABC(Opcode::CALL_rA_rB_rC, resultReg2, calleeBase2, funcReg2,
		Interp("call expr, result to r{}", resultReg2));

	// Free argument registers and funcref register
	for (Int32 i = 0; i < argCount2; i++) {
		FreeReg(argRegs2[i]);
	}
	FreeReg(funcReg2);

	return resultReg2;
}
Int32 CodeGeneratorStorage::Visit(MethodCallNode node) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	// Check if the target is 'super' — if so, preserve current self
	SuperNode superTarget = As<SuperNode, SuperNodeStorage>(node.Target());
	bool preserveSelf = (!IsNull(superTarget));

	// Evaluate receiver
	Int32 receiverReg = node.Target().Accept(_this);

	// Emit method call
	Int32 resultReg = EmitMethodCall(receiverReg, node.Method(), node.Arguments(), preserveSelf);

	FreeReg(receiverReg);
	return resultReg;
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

	// Get current element by position: varReg = iterget(listReg, indexReg)
	// For lists/strings this is the same as INDEX; for maps it returns {"key":k, "value":v}
	_emitter.EmitABC(Opcode::ITERGET_rA_rB_rC, varReg, listReg, indexReg, Interp("{} = iterget(container, index)", node.Variable()));

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

	// If the inner function uses self/super, ensure the outer function also
	// allocates those registers so ApplyPendingContext can populate them
	// and FUNCREF can capture them for the closure.
	if (funcDef.SelfReg() >= 0) GetSelfReg();
	if (funcDef.SuperReg() >= 0) GetSuperReg();

	// Store in the shared functions list
	_functions[funcIndex] = funcDef;

	// In the outer function, emit FUNCREF to create a reference
	_emitter.EmitAB(Opcode::FUNCREF_iA_iBC, resultReg, funcIndex,
		Interp("r{} = funcref {}", resultReg, funcName));

	GC_POP_SCOPE();
	return resultReg;
}
Int32 CodeGeneratorStorage::GetSelfReg() {
	FuncDef fd = _emitter.PendingFunc();
	if (fd.SelfReg() >= 0) return fd.SelfReg();
	Int32 reg = AllocReg();
	_variableRegs["self"] = reg;
	// Don't emit NAME here — ApplyPendingContext sets the name at runtime
	// when called as a method. If not called as a method, the name stays
	// empty so LOADV/LOADC will fall through to outer scope lookup (closures).
	fd.set_SelfReg((Int16)reg);
	return reg;
}
Int32 CodeGeneratorStorage::GetSuperReg() {
	FuncDef fd = _emitter.PendingFunc();
	if (fd.SuperReg() >= 0) return fd.SuperReg();
	Int32 reg = AllocReg();
	_variableRegs["super"] = reg;
	// Don't emit NAME here — see GetSelfReg comment.
	fd.set_SuperReg((Int16)reg);
	return reg;
}
Int32 CodeGeneratorStorage::Visit(SelfNode node) {
	Int32 resultReg = GetTargetOrAlloc();
	Int32 selfReg = GetSelfReg();
	Int32 nameIdx = _emitter.AddConstant(val_self);
	_emitter.EmitABC(Opcode::LOADV_rA_rB_kC, resultReg, selfReg, nameIdx,
		Interp("r{} = self", resultReg));
	return resultReg;
}
Int32 CodeGeneratorStorage::Visit(SuperNode node) {
	Int32 resultReg = GetTargetOrAlloc();
	Int32 superReg = GetSuperReg();
	Int32 nameIdx = _emitter.AddConstant(val_super);
	_emitter.EmitABC(Opcode::LOADV_rA_rB_kC, resultReg, superReg, nameIdx,
		Interp("r{} = super", resultReg));
	return resultReg;
}
Int32 CodeGeneratorStorage::EmitMethodCall(Int32 receiverReg, String methodKey, List<ASTNode> arguments, bool preserveSelf) {
	CodeGenerator _this(std::static_pointer_cast<CodeGeneratorStorage>(shared_from_this()));
	Int32 explicitTarget = _targetReg;
	_targetReg = -1;

	// Look up the method using METHFIND (walks __isa chain, sets pending self/super)
	Int32 keyReg = AllocReg();
	Int32 constIdx = _emitter.AddConstant(make_string(methodKey));
	_emitter.EmitAB(Opcode::LOAD_rA_kBC, keyReg, constIdx, Interp("r{} = \"{}\"", keyReg, methodKey));
	Int32 funcReg = AllocReg();
	_emitter.EmitABC(Opcode::METHFIND_rA_rB_rC, funcReg, receiverReg, keyReg,
		Interp("r{} = {} (method lookup)", funcReg, methodKey));
	FreeReg(keyReg);

	// For super.method() calls, override pendingSelf with the current self
	if (preserveSelf) {
		Int32 selfReg = GetSelfReg();
		_emitter.EmitA(Opcode::SETSELF_rA, selfReg, Interp("preserve self for super call"));
	}

	// Compile arguments
	Int32 argCount = arguments.Count();
	List<Int32> argRegs =  List<Int32>::New();
	for (Int32 i = 0; i < argCount; i++) {
		argRegs.Add(arguments[i].Accept(_this));
	}

	// Emit ARGBLK + ARG instructions
	_emitter.EmitABC(Opcode::ARGBLK_iABC, 0, 0, argCount, Interp("argblock {}", argCount));
	for (Int32 i = 0; i < argCount; i++) {
		_emitter.EmitA(Opcode::ARG_rA, argRegs[i], Interp("arg {}", i));
	}

	// Determine callee frame base
	Int32 calleeBase = _maxRegUsed + 1;
	_emitter.ReserveRegister(calleeBase);

	// Result register
	Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

	// Emit CALL
	_emitter.EmitABC(Opcode::CALL_rA_rB_rC, resultReg, calleeBase, funcReg,
		Interp("method call {}, result to r{}", methodKey, resultReg));

	// Free temporaries
	for (Int32 i = 0; i < argCount; i++) {
		FreeReg(argRegs[i]);
	}
	FreeReg(funcReg);

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
